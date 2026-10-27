<h1 align="center">AirPods for Oaxaca</h1>

<p align="center">
  Battery for each pod and the case, the listening modes, adaptive noise level, Conversation Awareness, One-Bud ANC and ear detection, in the Oaxaca bar.
</p>

<p align="center">
  <img src="preview.png" alt="The AirPods panel open in the bar" width="420">
</p>

This is a fork of [thisisgm/omarchy-pods](https://github.com/thisisgm/omarchy-pods)
for [Oaxaca](https://oaxaca.page), a port of Omarchy to Ubuntu 26.10. The panel is
upstream's, unchanged. The daemon is confined by an AppArmor profile instead of
systemd's sandbox, which does not take effect on Ubuntu; see
[What changed in the fork](#what-changed-in-the-fork). Upstream's README, with its
steps for Omarchy on Arch, is kept in [README.upstream.md](README.upstream.md); its
panel keyboard shortcuts, settings and notes on the output codec apply here too.

## Install

Install the plugin from Oaxaca's menu, on Oaxaca's desktop package
`oaxaca-desktop` 4.0.6+git20261027.146 or later; `oaxaca-update` brings an older
one up to date. The menu entry removes upstream's plugin (thisisgm/omarchy-pods)
if it is installed, adds this plugin, pairs the AirPods if they aren't paired yet,
and runs [setup](#setup). The AirPods' pairing and settings in
`~/.config/AirPodsTrayApp` carry over from upstream's plugin.

Pair the AirPods through this menu entry, not the Bluetooth panel. The Bluetooth
panel lists the AirPods' Bluetooth Low Energy addresses, and pairing one of those
does not pair the AirPods' audio link.

**Step 1.** Put the AirPods in the case with the lid open, close to the computer.

**Step 2.** Press Super+Space and choose **Install > Service > AirPods**. A
terminal window opens.

**Step 3.** If the AirPods aren't paired with this computer, the installer asks
"Pair AirPods with this computer now?". Answer yes. Make the case's status light
flash white: on AirPods Pro 3, double-tap the front of the case; on a case with a
setup button on the back, press and hold the button. Then answer yes to "Is the
light flashing white?" and keep the lid open while the installer looks for the
AirPods. The installer pairs and connects them on its own.

**Step 4.** [setup](#setup) asks for `sudo` twice: once to install the build
tools `cmake` and `ninja-build`, and once to install and load
[the AppArmor profile](#the-apparmor-profile). setup stops with an error before
building anything if AppArmor is not enabled, so [the daemon](#the-daemon) never
runs unconfined. The installer ends with "The AirPods daemon is running, confined
by its AppArmor profile."

**Step 5.** Optional. The icon stays hidden until AirPods are connected. To keep
it visible:

```bash
oaxaca bar set io.github.thisisgm.omapods hideWhenDisconnected false --json
```

## Update

`oaxaca plugin update` pulls the new commits but does not run
[setup](#setup), so run setup after it to rebuild the daemon and reload the
profile.

**Step 1.** Pull the new commits:

```bash
oaxaca plugin update io.github.thisisgm.omapods
```

**Step 2.** Rebuild the daemon and reload the profile:

```bash
~/.config/omarchy/plugins/io.github.thisisgm.omapods/setup
```

## Remove

**Step 1.** Press Super+Space and choose **Remove > Services > AirPods**. The
remover stops the daemon, deletes the files setup installed, unloads and deletes
the AppArmor profile (`sudo` asks you to authenticate), deletes the daemon's
settings and status, and removes the plugin. The daemon's settings in
`~/.config/AirPodsTrayApp` include the AirPods' pairing secrets (`magicAccIRK`,
`magicAccEncKey`).

**Step 2.** Optional. The remover leaves the AirPods paired with this computer.
To unpair them, press Super+Ctrl+B to open the Bluetooth panel, hover over the
AirPods' row and click the button whose tooltip reads **Forget**, or select the
row and press `x`.

## Terms

### Oaxaca

A Linux distribution that ports Omarchy's desktop (Hyprland and Omarchy's bar and
panels) to Ubuntu 26.10. Its commands are `oaxaca …`, and `omarchy …` runs the same
commands.

### The panel

The bar widget: the AirPods icon in the bar and the panel that opens when you click
it, written in QML (`Panel.qml`, `AirPodsIcon.qml`, `Model.js`). It only reads a
status file and sends commands; it never talks to Bluetooth.

### The daemon

`librepods`, a modified copy of [librepods](https://github.com/kavishdevar/librepods)
in [`daemon/`](daemon/). It talks to the AirPods over Bluetooth, writes their
status to `~/.local/state/librepods/status.json` for [the panel](#the-panel), and
takes the panel's commands through `librepods-ctl`. It is installed to
`~/.local/bin/librepods` and run by the systemd user service `librepods.service`
(`~/.local/share/systemd/user/librepods.service`).

### setup

The script `setup` at the top of this repository. `oaxaca plugin add` only clones
the repository, and `setup` does the rest: it builds [the daemon](#the-daemon),
installs it, installs [the AppArmor profile](#the-apparmor-profile) and starts the
service.

### The AppArmor profile

[`daemon/apparmor/librepods`](daemon/apparmor/librepods), the AppArmor rules that
limit which files, programs and D-Bus calls [the daemon](#the-daemon) can reach.
[setup](#setup) installs it to `/etc/apparmor.d/librepods`. AppArmor attaches it
by path to every process started from `~/.local/bin/librepods`.

## What it shows

- **Battery** for the left pod, the right pod and the case, each with a charging
  and in-ear hint. BlueZ does not expose `org.bluez.Battery1` for AirPods, so
  nothing else on the system knows these numbers.
- **Listening mode**, only the modes the model has.
- **Adaptive noise level**, while Adaptive is the active mode.
- **Conversation Awareness** and **One-Bud ANC** on the models that have them.
  One-Bud ANC keeps noise cancellation on with a single pod in your ear; with both
  in, it changes nothing.
- **Ear detection**: pause when one pod is out, pause when both are out, or never
  pause.
- **Case lid**, from the case's Bluetooth advertisements. The lid state holds its
  last value while the pods stay connected.

Volume, output device, connect, disconnect and forget are left to Oaxaca's own
Audio and Bluetooth panels and to `oaxaca bluetooth device`.

## Screenshots

| | | |
|:---:|:---:|:---:|
| <img src="docs/panel-model-airpods4.png" alt="AirPods 4 with ANC"><br>**AirPods 4 with ANC** | <img src="docs/panel-model-pro3.png" alt="AirPods Pro 3"><br>**AirPods Pro 3**, which has no Off mode | <img src="docs/panel-model-max2.png" alt="AirPods Max 2"><br>**AirPods Max 2**, one battery and no case |
| <img src="docs/panel-noise-cancellation.png" alt="Noise Cancellation"><br>Noise Cancellation | <img src="docs/panel-adaptive.png" alt="Adaptive"><br>Adaptive, with the noise level | <img src="docs/panel-in-case.png" alt="Both pods in the case"><br>Both pods charging, lid closed |

The screenshots are upstream's, taken on Omarchy.

## What changed in the fork

### Why systemd's sandbox does not work on Oaxaca

Upstream's `librepods.service` sandboxes the daemon with systemd settings. On
Oaxaca, those settings either stop the service from starting or are silently
skipped:

- **Capability settings fail the start.** `CapabilityBoundingSet=`,
  `ProtectKernelModules=yes`, `ProtectKernelLogs=yes` and `ProtectClock=yes` each
  remove capabilities from the process, which a user's systemd instance has no
  privilege to do. The start fails with `status=218/CAPABILITIES` ("Failed to drop
  capabilities: Operation not permitted"), and the service restarts every 5
  seconds. A user service holds none of those capabilities anyway.
- **Namespace settings are skipped.** `ProtectSystem`, `ProtectHome`,
  `ReadWritePaths`, `PrivateTmp`, `ProtectHostname`, `ProtectKernelTunables` and
  `ProtectControlGroups` need a private mount namespace. A user's systemd instance
  can only create one inside a user namespace, and Ubuntu restricts those
  (`kernel.apparmor_restrict_unprivileged_userns = 1`). The process that creates
  one is moved into Ubuntu's `unprivileged_userns` AppArmor profile, which denies
  the capability the mounts need, so systemd starts the daemon without them. The
  daemon then runs in the same mount namespace as the desktop, with all of home
  writable. Combined with [the AppArmor profile](#the-apparmor-profile), the
  namespace settings also make the daemon run under the stacked label
  `librepods//&unprivileged_userns`, which denies its system bus calls.
- **D-Bus was never filtered.** The daemon needs both buses: the system bus for
  BlueZ and logind, the session bus for media players and notifications. Without
  a filter, the session bus lets any process ask the user's systemd to start a
  command (`StartTransientUnit`), and that command runs outside every sandbox.
  This holds on any distribution, with or without the namespace settings.

### Changes

| File | Change |
|---|---|
| [`daemon/apparmor/librepods`](daemon/apparmor/librepods) | New: [the AppArmor profile](#the-apparmor-profile). |
| [`daemon/librepods.service`](daemon/librepods.service) | The capability and namespace settings are removed. `NoNewPrivileges`, `RestrictSUIDSGID`, `RestrictNamespaces`, `LockPersonality`, `SystemCallArchitectures`, `RestrictAddressFamilies=AF_UNIX AF_BLUETOOTH AF_NETLINK` (no IP networking) and `UMask=0077` stay; they work without namespaces. |
| [`daemon/media/mediacontroller.cpp`](daemon/media/mediacontroller.cpp) | The WirePlumber restart is a D-Bus `RestartUnit` call on the session bus instead of `systemctl --user restart wireplumber`. `systemctl --user` uses systemd's private socket, which the profile denies. |
| [`setup`](setup) | Stops if AppArmor is not enabled, then installs the profile to `/etc/apparmor.d/librepods` and loads it with `apparmor_parser -r`. |

### What the profile allows

- **Files:**
  - read the system's configuration, PulseAudio's client settings,
    `/etc/machine-id`, the boot ID and other processes' command lines (Qt reads
    `bluetoothd`'s to find its version);
  - read and write only `~/.config/AirPodsTrayApp/`, `~/.local/state/librepods/`
    and the control socket `/run/user/<uid>/librepods.sock`;
  - nothing else in home.
- **Sockets:** Bluetooth, netlink and local Unix sockets. The system bus, the
  session bus and PulseAudio's socket are allowed. systemd's private socket
  `/run/user/<uid>/systemd/private` is denied, because it reaches systemd's
  whole API without passing through the D-Bus rules.
- **Programs**, each confined by the same profile: `bluetoothctl`,
  `bluetoothd --version`, and `oaxaca notification send` with the shell and tools
  its scripts use.
- **D-Bus calls:**
  - system bus: BlueZ's discovery, connect, disconnect, remove and profile
    registration calls, BlueZ's and logind's properties;
  - session bus: media players' MPRIS calls (pause and resume on ear detection),
    desktop notifications, and systemd's `RestartUnit`;
  - nothing that starts a new unit or command.

Rules for one machine go in `/etc/apparmor.d/local/librepods`, which the profile
includes if it exists and [setup](#setup) does not overwrite.

### How the profile was checked

On Oaxaca 26.10 with AirPods Pro 3, on 2026-10-26. The rules were written from
what AppArmor logged in complain mode (which logs accesses without blocking them)
while every panel control, ear detection, the case lid and a reconnect were used.
Then, in enforce mode, each row below ran under the profile with
`aa-exec -p librepods`:

| Check | Result |
|---|---|
| Read `~/.bashrc` | denied |
| Write to `~/.cache` | denied |
| `StartTransientUnit` over the session bus, to run `touch` outside the profile | denied (`Call failed: Access denied`) |
| Open a TCP connection to `1.1.1.1:443` | denied |
| `RestartUnit` on a test unit | allowed, the unit restarted |
| `oaxaca notification send` | allowed, exit status 0 |
| `bluetoothctl devices Connected` | allowed, the AirPods were listed |
| The daemon itself | `librepods (enforce)`, AirPods connected, no denials logged |

## Credits

[librepods](https://github.com/kavishdevar/librepods) by **Kavish Devar**
reverse-engineered Apple's AAP protocol over L2CAP and the Bluetooth Low Energy
advertisements that carry battery, in-ear and case lid state. The daemon in
`daemon/` is a modified copy of his work.

The panel, and the daemon's changes recorded in
[daemon/UPSTREAM.md](daemon/UPSTREAM.md), are by **GM** in
[thisisgm/omarchy-pods](https://github.com/thisisgm/omarchy-pods). You can
[buy GM a coffee](https://buymeacoffee.com/thisisgm).

## Licence

Two programs live here, licensed separately because they are separate works that
talk over a status file and a command line.

| Path | Licence | |
|---|---|---|
| repository root, the bar widget | MIT | [LICENSE](LICENSE) |
| `daemon/`, a modified copy of librepods | GPL-3.0 | [daemon/LICENSE](daemon/LICENSE) |

Shipping both in one repository is aggregation, not combination, so the widget
stays MIT and the daemon stays GPL-3.0. What was modified, and the upstream commit
it was forked from, are recorded in [daemon/UPSTREAM.md](daemon/UPSTREAM.md).

The three product outlines in `AirPodsIcon.qml` are Apple's, taken from the
chapter navigation on [apple.com/airpods](https://www.apple.com/airpods/).
AirPods, AirPods Pro and AirPods Max are trademarks of Apple Inc., which does not
sponsor or endorse this plugin. Those outlines are not this project's to license,
so the MIT grant above does not reach them.
