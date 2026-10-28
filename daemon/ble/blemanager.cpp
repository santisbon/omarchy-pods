#include "blemanager.h"
#include "logger.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>

namespace {
const QString bluezService = QStringLiteral("org.bluez");
const QString monitorManagerInterface = QStringLiteral("org.bluez.AdvertisementMonitorManager1");
const QString monitorInterface = QStringLiteral("org.bluez.AdvertisementMonitor1");
const QString deviceInterface = QStringLiteral("org.bluez.Device1");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString monitorRootPath = QStringLiteral("/org/librepods/monitor");
const QString monitorPath = QStringLiteral("/org/librepods/monitor/airpods");
constexpr quint16 appleCompanyId = 0x004C;
constexpr quint8 manufacturerDataType = 0xFF;
// Apple's company id, little-endian, then the proximity pairing message type the parser reads.
const QByteArray airPodsAdvertisementPrefix = QByteArray::fromHex("4c0007");
}

// One entry of the monitor's Patterns property, D-Bus signature (yyay).
struct MonitorPattern
{
    quint8 start = 0;
    quint8 adType = 0;
    QByteArray content;
};
using MonitorPatterns = QList<MonitorPattern>;
using InterfaceProperties = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceProperties>;
Q_DECLARE_METATYPE(MonitorPattern)
Q_DECLARE_METATYPE(InterfaceProperties)
Q_DECLARE_METATYPE(ManagedObjects)

QDBusArgument &operator<<(QDBusArgument &argument, const MonitorPattern &pattern)
{
    argument.beginStructure();
    argument << pattern.start << pattern.adType << pattern.content;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MonitorPattern &pattern)
{
    argument.beginStructure();
    argument >> pattern.start >> pattern.adType >> pattern.content;
    argument.endStructure();
    return argument;
}

static MonitorPatterns airPodsPatterns()
{
    return {MonitorPattern{0, manufacturerDataType, airPodsAdvertisementPrefix}};
}

// BlueZ reads the monitor's properties from this object's GetManagedObjects when RegisterMonitor names it.
class AdvertisementMonitorRoot : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")
public:
    using QObject::QObject;

public slots:
    ManagedObjects GetManagedObjects()
    {
        QVariantMap properties;
        properties.insert(QStringLiteral("Type"), QStringLiteral("or_patterns"));
        properties.insert(QStringLiteral("Patterns"), QVariant::fromValue(airPodsPatterns()));
        ManagedObjects objects;
        objects.insert(QDBusObjectPath(monitorPath), {{monitorInterface, properties}});
        return objects;
    }
};

class AdvertisementMonitor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.AdvertisementMonitor1")
public:
    using QObject::QObject;

public slots:
    void Release() { emit released(); }
    void Activate() { LOG_INFO("BlueZ activated the AirPods advertisement monitor"); }
    void DeviceFound(const QDBusObjectPath &device) { emit deviceFound(device.path()); }
    void DeviceLost(const QDBusObjectPath &device) { emit deviceLost(device.path()); }

signals:
    void released();
    void deviceFound(const QString &devicePath);
    void deviceLost(const QString &devicePath);
};

BleManager::BleManager(QObject *parent) : QObject(parent)
{
    qDBusRegisterMetaType<MonitorPattern>();
    qDBusRegisterMetaType<MonitorPatterns>();
    qDBusRegisterMetaType<InterfaceProperties>();
    qDBusRegisterMetaType<ManagedObjects>();

    m_root = new AdvertisementMonitorRoot(this);
    m_monitor = new AdvertisementMonitor(this);
    connect(m_monitor, &AdvertisementMonitor::released, this, &BleManager::onMonitorReleased);
    connect(m_monitor, &AdvertisementMonitor::deviceFound, this, &BleManager::onDeviceFound);
    connect(m_monitor, &AdvertisementMonitor::deviceLost, this, &BleManager::onDeviceLost);
}

BleManager::~BleManager()
{
    stopScan();
}

void BleManager::startScan()
{
    if (m_registered)
        return;

    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusMessage objectsCall = QDBusMessage::createMethodCall(
        bluezService, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("GetManagedObjects"));
    QDBusReply<ManagedObjects> objects = bus.call(objectsCall);
    if (!objects.isValid())
    {
        LOG_ERROR("BLE monitor: cannot list BlueZ objects:" << objects.error().message());
        return;
    }
    m_adapterPath.clear();
    for (auto it = objects.value().cbegin(); it != objects.value().cend(); ++it)
    {
        if (it.value().contains(monitorManagerInterface))
        {
            m_adapterPath = it.key().path();
            break;
        }
    }
    if (m_adapterPath.isEmpty())
    {
        LOG_ERROR("BLE monitor: no Bluetooth adapter offers" << monitorManagerInterface);
        return;
    }

    if (!bus.registerObject(monitorRootPath, m_root, QDBusConnection::ExportAllSlots)
        || !bus.registerObject(monitorPath, m_monitor, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals))
    {
        LOG_ERROR("BLE monitor: cannot export" << monitorRootPath << "on the system bus:" << bus.lastError().message());
        unregisterObjects();
        return;
    }

    LOG_DEBUG("Registering the AirPods advertisement monitor on" << m_adapterPath);
    QDBusMessage registerCall = QDBusMessage::createMethodCall(
        bluezService, m_adapterPath, monitorManagerInterface, QStringLiteral("RegisterMonitor"));
    registerCall << QVariant::fromValue(QDBusObjectPath(monitorRootPath));
    // Asynchronous, because BlueZ calls GetManagedObjects on this process before it replies.
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(registerCall), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &BleManager::onRegisterFinished);
    m_registered = true;
}

void BleManager::stopScan()
{
    if (!m_registered)
        return;

    LOG_DEBUG("Unregistering the AirPods advertisement monitor");
    QDBusMessage unregisterCall = QDBusMessage::createMethodCall(
        bluezService, m_adapterPath, monitorManagerInterface, QStringLiteral("UnregisterMonitor"));
    unregisterCall << QVariant::fromValue(QDBusObjectPath(monitorRootPath));
    QDBusConnection::systemBus().call(unregisterCall, QDBus::NoBlock);
    unregisterObjects();
}

bool BleManager::isScanning() const
{
    return m_registered;
}

void BleManager::onRegisterFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError())
    {
        LOG_ERROR("BLE monitor: RegisterMonitor on" << m_adapterPath << "failed:" << reply.error().name() << reply.error().message());
        unregisterObjects();
    }
}

void BleManager::onMonitorReleased()
{
    LOG_WARN("BLE monitor: BlueZ released the AirPods advertisement monitor");
    unregisterObjects();
}

void BleManager::onDeviceFound(const QString &devicePath)
{
    if (m_devices.contains(devicePath))
        return;
    LOG_DEBUG("BLE monitor matched" << devicePath);
    m_devices.insert(devicePath, {});
    QDBusConnection::systemBus().connect(bluezService, devicePath, propertiesInterface, QStringLiteral("PropertiesChanged"),
                                         this, SLOT(onDevicePropertiesChanged(QDBusMessage)));
    QDBusMessage getAll = QDBusMessage::createMethodCall(bluezService, devicePath, propertiesInterface, QStringLiteral("GetAll"));
    getAll << deviceInterface;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(getAll), this);
    watcher->setProperty("devicePath", devicePath);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &BleManager::onDeviceProperties);
}

void BleManager::onDeviceLost(const QString &devicePath)
{
    unwatchDevice(devicePath);
}

void BleManager::onDeviceProperties(QDBusPendingCallWatcher *watcher)
{
    const QString devicePath = watcher->property("devicePath").toString();
    QDBusPendingReply<QVariantMap> reply = *watcher;
    watcher->deleteLater();
    if (!m_devices.contains(devicePath))
        return;
    if (reply.isError())
    {
        LOG_WARN("BLE monitor: cannot read" << devicePath << ":" << reply.error().message());
        return;
    }
    const QVariantMap properties = reply.value();
    WatchedDevice &device = m_devices[devicePath];
    device.address = properties.value(QStringLiteral("Address")).toString();
    device.name = properties.value(QStringLiteral("Alias")).toString();
    if (properties.contains(QStringLiteral("ManufacturerData")))
        parseAppleData(devicePath, properties.value(QStringLiteral("ManufacturerData")));
}

void BleManager::onDevicePropertiesChanged(const QDBusMessage &message)
{
    // Sample input: ("org.bluez.Device1", {"ManufacturerData": {76: <ay>}, "RSSI": -60}, [])
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2 || arguments.at(0).toString() != deviceInterface || !m_devices.contains(message.path()))
        return;
    const QVariantMap changed = qdbus_cast<QVariantMap>(arguments.at(1));
    if (changed.contains(QStringLiteral("ManufacturerData")))
        parseAppleData(message.path(), changed.value(QStringLiteral("ManufacturerData")));
}

void BleManager::parseAppleData(const QString &devicePath, const QVariant &manufacturerData)
{
    // Sample input: a{qv} {76: <[0x07, 0x19, 0x01, 0x27, 0x20, ...]>}
    QByteArray appleData;
    const QDBusArgument argument = manufacturerData.value<QDBusArgument>();
    argument.beginMap();
    while (!argument.atEnd())
    {
        quint16 companyId = 0;
        QDBusVariant value;
        argument.beginMapEntry();
        argument >> companyId >> value;
        argument.endMapEntry();
        if (companyId == appleCompanyId)
            appleData = value.variant().toByteArray();
    }
    argument.endMap();

    const WatchedDevice &device = m_devices.value(devicePath);
    BleInfo info;
    if (parseProximityPairing(device.address, device.name, appleData, &info))
        emit deviceFound(info);
}

void BleManager::unwatchDevice(const QString &devicePath)
{
    if (m_devices.remove(devicePath) == 0)
        return;
    QDBusConnection::systemBus().disconnect(bluezService, devicePath, propertiesInterface, QStringLiteral("PropertiesChanged"),
                                            this, SLOT(onDevicePropertiesChanged(QDBusMessage)));
}

void BleManager::unregisterObjects()
{
    const QStringList devicePaths = m_devices.keys();
    for (const QString &devicePath : devicePaths)
        unwatchDevice(devicePath);
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.unregisterObject(monitorPath);
    bus.unregisterObject(monitorRootPath);
    m_registered = false;
}

#include "blemanager.moc"
