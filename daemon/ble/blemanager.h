#ifndef BLEMANAGER_H
#define BLEMANAGER_H

#include <QObject>
#include <QHash>
#include <QString>
#include "proximitypairing.h"

class QDBusMessage;
class QDBusPendingCallWatcher;
class AdvertisementMonitorRoot;
class AdvertisementMonitor;

// A BlueZ advertisement monitor, not discovery, which would stop the background scan bonded keyboards reconnect through.
class BleManager : public QObject
{
    Q_OBJECT
public:
    explicit BleManager(QObject *parent = nullptr);
    ~BleManager();

    void startScan();
    void stopScan();
    bool isScanning() const;

signals:
    void deviceFound(const BleInfo &device);

private slots:
    void onRegisterFinished(QDBusPendingCallWatcher *watcher);
    void onMonitorReleased();
    void onDeviceFound(const QString &devicePath);
    void onDeviceLost(const QString &devicePath);
    void onDeviceProperties(QDBusPendingCallWatcher *watcher);
    void onDevicePropertiesChanged(const QDBusMessage &message);

private:
    struct WatchedDevice
    {
        QString address;
        QString name;
    };

    void unwatchDevice(const QString &devicePath);
    void unregisterObjects();
    void parseAppleData(const QString &devicePath, const QVariant &manufacturerData);

    AdvertisementMonitorRoot *m_root = nullptr;
    AdvertisementMonitor *m_monitor = nullptr;
    QString m_adapterPath;
    bool m_registered = false;
    QHash<QString, WatchedDevice> m_devices;
};

#endif // BLEMANAGER_H
