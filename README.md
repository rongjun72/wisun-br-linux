[<img align="right" src="misc/wisun-logo.png" width="300" alt="Wi-SUN Logo">][0]

Wi-SUN Linux Border Router
==========================

The goal of this project is to implement the [Wi-SUN protocol][0] on Linux
devices and allow the use of Linux hosts as Border Routers for Wi-SUN networks.
For the time being, the implementation is mostly a port of Silicon Labs'
embedded stack on a Linux host. However, the ultimate goal is to replace
services currently provided by the stack with native Linux services.

[0]: https://wi-sun.org/

# Quick Start Guide

## Prerequisites

This project provides the `wsbrd` daemon, which is responsible for running the
Wi-SUN protocol high-level layers. It is paired with an RF device RCP (Radio
Co-Processor) handling the low-level layers and RF activities. The RCP devices
currently supported are the EFR32FG12 and EFR32MG12.

The RCP needs to be flashed with a specific firmware to communicate with the
daemon. This firmware is provided in binary format. To help users deploy and
evaluate the solution, a [wisun-br-linux-docker][1] repository is provided. It
contains a bundle of all the necessary software components (including a compiled
RCP firmware image) to run the Linux Wi-SUN border router.

Alternatively, [Application Note 1332][2] explains how to build RCP firmware and
flash it.

The communication between the Linux host and the RCP is supported through a
serial link (UART). On Silicon Labs mainboards, this serial link is provided
over USB. The `/dev/ACMx` device should appear when you connect the mainboard.

[1]: https://github.com/SiliconLabs/wisun-br-linux-docker
[2]: https://www.silabs.com/documents/public/application-notes/an1332-wi-sun-network-configuration.pdf

## Cloning wisun-br-linux

If it is not yet done, start by cloning this repository:

    git clone https://github.com/SiliconLabs/wisun-br-linux.git

## Compiling

The build requires `mbedTLS` (> 2.18), `libnl-3`, `libnl-route-3`, and `cmake`.
`libcap` and `libsystemd` are also recommended (note that `libsystemd` can be
replaced by `elogind` if you do not want to pull `systemd`). Optionally, you can
also install `libpcap` and Rust/Cargo.

We also encourage the use of Ninja as the `cmake` back-end.

On Debian and its derivatives, install the necessary dependencies (except for
mbedTLS) with:

    sudo apt-get install libnl-3-dev libnl-route-3-dev libcap-dev libpcap-dev \
        libsystemd-dev libdbus-1-dev cargo cmake ninja-build pkg-config lrzsz

Debian does not (yet) package `mbedTLS` > 2.18 so you must build it from
sources. Note that support for `cmake` has been added to `mbedTLS` 2.27. So, if
you want to use `mbedTLS` < 2.27, the following process does not work. In
addition, since `wsbrd` is mainly tested with `mbedTLS` 3.0, we suggest using
this version.

    git clone --branch=v3.0.0 https://github.com/ARMmbed/mbedtls
    cd mbedtls
    cmake -G Ninja .
    ninja
    sudo ninja install

`MbedTLS` is highly customizable. The default configuration is sane. However, if
you want a stripped-down version, you can configure it with the configuration
file provided in `examples/mbedtls-config.h`:

    CFLAGS="-I$FULL_PATH_TO_WSBRD_SRC/examples -DMBEDTLS_CONFIG_FILE='<mbedtls-config.h>'" cmake -G Ninja .

> This configuration file has been written for `mbedtls` 3.0. Adapt it if
> necessary.

Optionally, `wsbrd` can be compiled with support for [Silabs
CPC](#should-i-use-cpc-or-plain-uart). To install Silabs CPC library:

    git clone https://github.com/SiliconLabs/cpc_daemon.git
    cd cpc_daemon
    cmake -S . -B build -G Ninja
    ninja -C build
    sudo ninja -C build install
    sudo ldconfig

Then, you can compile `wsbrd` with:

    cd wisun-br-linux/
    cmake -G Ninja .
    ninja

Finally, install the service with:

    sudo ninja install

> No script for any start-up service is provided for now.

## Launching

You must provide a configuration file to the Wi-SUN border router. A commented
example is available in `/usr/local/share/doc/wsbrd/examples/wsbrd.conf`.

    cp -r /usr/local/share/doc/wsbrd/examples .
    <edit examples/wsbrd.conf>

You can copy and edit it. You will notice that you need certificates and keys to
authenticate your network's Wi-SUN nodes. The generation of these files is
described in [[Generate Wi-SUN PKI]].  For now, you can use the certificate
examples installed in `/usr/local/share/doc/wsbrd/examples/`.

You also must provide the path of the UART representing your RCP device.

Finally, launch `wsbrd` with:

    sudo wsbrd -F examples/wsbrd.conf -u /dev/ttyACM0

`wsbrd` lists the useful options in the output of `wsbrd --help`.

# Tools

A suite of tools is provided for various tasks around `wsbrd` and its RCP. For
more detail, refer to the `README.md` present in the relevant source folder
(under [`tools/`](tools)), or the `--help` message output by the application.
Some of these are not compiled by default and require setting
`COMPILE_DEVTOOLS=ON` when configuring the project with CMake.

| Application  | Description                                           |
|--------------|-------------------------------------------------------|
| `wsbrd_cli`  | A simple application for querying the D-Bus interface |
| `wsbrd-fwup` | A tool for updating the RCP firware                   |
| `wsbrd-fuzz` | A tool for fuzzing and debugging `wsbrd`              |
| `wshwping`   | A tool for testing the serial link                    |

# Using `wsbrd_cli` and the D-Bus Interface

`wsbrd_cli` is a small utility to retrieve the status of the Wi-SUN network. Its
usage is described in the output of `wsbrd_cli --help`. The tool relies on the
D-Bus interface provided by `wsbrd`, which is described in `DBUS.md`.

# Generating the Wi-SUN Public Key Infrastructure

The certificate generation process is described in section 6.5.1 of the Wi-SUN
specification. It uses the standard X.509 certificate format. Some fields and
algorithms are enforced.

The process to get official certificates is described on the [Wi-SUN alliance
Web site][7] (restricted access).

[7]: https://wi-sun.org/cyber-security-certificates/

# Using an External DHCPv6 Server

`wsbrd` provides a built-in DHCPv6 server. However, it is still possible to use
an external DHCPv6 server. If the DHCP server runs on a remote host, you need to
launch a DHCPv6 relay.

`wsbrd` has been tested with ISC DHCP and dnsmasq. Both projects provide DHCP
server and DHCP relay implementations.

First you have to deactivate the internal dhcp server of `wsbrd` in wbsrd.conf:

    internal_dhcp = false

Obviously, the DHCP server/relay needs a network interface to run. You can
launch the DHCP server/relay just after `wsbrd` (you have to ensure the DHCP
service is started before Wi-SUN nodes connect, but they will not connect before
at least several dozen seconds) or [create the interface before launching
`wsbrd`](#running-wsbrd-without-root-privilege).

## Using `dnsmasq`

Because of [this issue][9], `dnsmasq` is supported from the version 2.87.

[9]: https://www.mail-archive.com/dnsmasq-discuss@lists.thekelleys.org.uk/msg16394.html

### Using `dnsmasq` as DHCP Server

`dnsmasq` does not need any specific options. A classical invocation can be
used. We suggest increasing the lease time (`336h`) and disabling DNS server
(`-p 0`):

    sudo dnsmasq -d -C /dev/null -p 0 -i tun0 --dhcp-range 2001:db8::,2001:db8::ffff,64,336h

### Using `dnsmasq` as DHCP Relay

To start the DHCP relay, you have to bind the `Wi-SUN` and the upstream network
interfaces (`tun0` and `eth0`). Then specify the IP addresses of the DHCP server
(`2001:db8:a::1`) and of the `wsbrd` interface (`2001:db8:b::1`) to the
`--dhcp-relay` option. You can also disable the DNS server (which is useless in
this case) with `-p 0`:

    sudo dnsmasq -d -C /dev/null -p 0 -i tun0,eth0 --dhcp-relay 2001:db8:a::1,2001:db8:b::1

## Using `isc-dhcp`

Note that ISC DHCP needs to be patched to comply with Wi-SUN specification (for
relay and server). We provide the needed patches in `misc/`.

### Using `isc-dhcpd` as DHCP Server

`isc-dhcpd` will not start if the lease file does not exist:

    sudo mkdir -p /var/lib/dhcpd/
    sudo touch /var/lib/dhcpd/dhcpd.leases

We provide a sample configuration file for isc-dhcp. See `examples/dhcpd.conf`
for details:

    sudo dhcpd -6 --no-pid -lf /var/lib/dhcpd/dhcpd.leases -cf examples/dhcpd.conf tun0

### Using `isc-dhcrelay` as DHCP Relay

To start the DHCP relay, you have to provide the `wsbrd` network interface
(`tun0`), the address of the DHCP server (`2001:db8::1` in the example below),
and the network interface associated with this address (see [dhcprelay
manpage][8]):

    sudo dhcrelay -6 --no-pid -l tun0 -u 2001:db8::1%eth0

[8]: https://linux.die.net/man/8/dhcrelay

# Running `wsbrd` Without Root Privilege

To run `wsbrd` without root permissions, first ensure you have permission to
access the UART device (you will have to logout/login after this command):

    sudo usermod -aG dialout YOUR_USER

Then, you have to take over the creation of the network interface. This process
can also be useful to set up unusual configurations, or if you need to access
tun interface before `wsbrd` is launched.

First, create the network interface to give your user the permission to use
it:

    sudo ip tuntap add mode tun tun0 user YOUR_USER

The MTU must be set to 1280 bytes to comply with 802.15.4g:

    sudo ip link set dev tun0 mtu 1280

Silicon Labs suggests reducing the queue size of the interface to avoid huge
latencies:

    sudo ip link set dev tun0 txqueuelen 10

The Wi-SUN interface cannot be configured through SLAAC, so do not pollute your
network with unnecessary Router Solicitations:

    sudo sysctl net.ipv6.conf.tun0.accept_ra=0

Wi-SUN needs a link-address matching the EUI64 of the node. Therefore, Linux
should not generate any link-local address by itself.

    sudo sysctl net.ipv6.conf.tun0.addr_gen_mode=1

Then, `wsbrd` can automatically set up the IP addresses (Global and Link-Local)
of the interface. However, to run without root privileges, you have to do it
yourself.

Disable the `tun_autoconf` parameter in `wsbrd`'s configuration. Then add IP
addresses:

    sudo ip addr add dev tun0 fe80::200:5eef:1000:1/64
    sudo ip addr add dev tun0 2001:db8::200:5eef:1000:1/64

The 64 least significant bits of these addresses must match with the EUI-64 of
the RCP (you can check logs of `wsbrd` to find it).

The network mask of the GUA must match with the `ipv6_prefix` parameter.

Finally, bring up the interface:

    sudo ip link set dev tun0 up

Also note, the internal DHCP will not be able to bind ports 546 and 547 without
root privilege. You can run an external DHCP server (with `internal_dhcp=false`)
or you can configure your system to allow normal users to bind port 546 and
above:

    sudo sysctl net.ipv4.ip_unprivileged_port_start=546

Finally, you can run `wsbrd`.

# Using IPv6 Transparent Proxy

Transparent IPv6 proxy provides IPv6 connectivity to the Wi-SUN
network without changing configuration of existing IPv6 infrastructure.
Once enabled:
   - The Wi-SUN nodes will appear as classical hosts on the network.
   - The other hosts on the network will be able to reach them.
   - The Wi-SUN nodes will be able to reach the Internet through the gateway of
     the host.
   - If the upstream gateway provides global addresses and there is no firewall
     on the way (which is uncommon), hosts on the Internet can reach the Wi-SUN
     nodes.

To enable this feature:
   - The `neighbor_proxy` parameter must be set to the name of the upstream
     network interface.
   - The `ipv6_prefix` parameter must be set to the same prefix as the hosting
     network.
   - IPv6 forward must be enabled on the host (with `sysctl
     net.ipv6.conf.all.forwarding=1`). Note that [enabling forwarding per
     interface does not work][1].


Under the hood, when `neighbor_proxy` is in use:
   - NDP proxy (`/proc/sys/net/ipv6/conf/*/proxy_ndp`) is enabled.
   - Wi-SUN nodes are automatically added to the neighbor proxy list (user can
     dump them with `ip -6 neigh show proxy`).
   - IPv6 routes are automatically added for the Wi-SUN nodes (user can dump
     them with `ip -6 route show`).
   - The delay before answering multicast neighbor solicitations
     (`/proc/sys/net/ipv6/neigh/*/proxy_delay`) is set to 0.


[9]: https://docs.kernel.org/networking/ip-sysctl.html#proc-sys-net-ipv6-variables

# Bugs and Limitations

## Should I Use CPC or Plain UART?

CPC protocol relies on an external service (CPCd). Therefore, plain UART allows
an easier integration for simple setups. However, CPC offers some features:

  - Support for SPI bus.
  - Support for encrypted link with the RCP.
  - Support for Dynamic MultiProtocol (DMP). Thus, CPCd can share the RCP
    between several network stacks (that is, Bluetooth, Zigbee, OpenThread, and
    Wi-SUN)

## I get `error inflating zlib stream; class=Zlib (5)` During Compilation

The last update of GitHub seems incompatible with the git version bundled with
Rust 1.45. The issue and the workaround are described [here][1] and the root
cause is solved [here][2].

Before launching `cmake`, you can run:

   export CARGO_NET_GIT_FETCH_WITH_CLI=true

[10]: https://github.com/rust-lang/cargo/issues/10303
[11]: https://github.com/libgit2/libgit2/pull/5740

## I Cannot Connect to DBus Interface

First, check you have followed the installation process. Especially, check you
have run `ninja install`.

There are several DBus instances on your system:
  - One system instance
  - An instance for each user

By default, `wsbrd` tries to use the `user` instance and falls back to `system`
instance.

The DBus session used is shown in the first lines of the log output:

    Successfully registered to system DBus

Then, use `busctl --system` or `busctl --user` accordingly.

Note that if you use `sudo` to launch `wsbrd` as root user, it will use the
`system` instance.

You can enforce the session used with an environment variable
`DBUS_STARTER_BUS_TYPE=system` or `DBUS_STARTER_BUS_TYPE=user`. If you use
`sudo`, you must define this variable inside the `sudo` environment:

    sudo env DBUS_STARTER_BUS_TYPE=system wsbrd ...

## LFNs and the D-Bus interface

Low function nodes (LFN) that are connected directly to the border router do
not appear as having parent when querrying the D-Bus `Nodes` API. This is due
to them being routed differently, and will be fixed in an future version. Tools
like `wsbrd_cli` and the web GUI that rely on this D-Bus API are affected by
this limitation.

## I Have Issues when Trying to Send UDP Data

Path MTU Discovery works as expected on the Wi-SUN network. The Border Router
replies with `ICMPv6/Packet Too Big` if necessary. (Remember that in IPv6,
routers cannot fragment packets, therefore the sender is responsible for the
size of the packet). Direct neighbors of the Border Router can receive frames up
to 1504 bytes, while the other nodes can receive frames up to 1280 bytes.

If you try to send a UDP frame larger than the MTU, there are two
options:

  - The packet has been sent with `IPV6_DONTFRAG`, and the operating system will
    return an error.
  - The packet is not marked with `IPV6_DONTFRAG`, and the operating system will
    fragment the packet.

On the receiver, the buffer must be large enough (up to 64 kB) to handle the
fragmented packet. This feature is sometimes limited on embedded devices.
[IPv6][12] requires at least 1500 bytes available during reception, and warns
on sending more:

> A node must be able to accept a fragmented packet that, after reassembly, is
> as large as 1500 octets.  A node is permitted to accept fragmented packets
> that reassemble to more than 1500 octets. An upper-layer protocol or
> application that depends on IPv6 fragmentation to send packets larger than
> the MTU of a path should not send packets larger than 1500 octets unless it
> has assurance that the destination is capable of reassembling packets of that
> larger size.

Typically, on Silicon Labs nodes, the default fragmentation buffer size is 1504
bytes. Therefore, if you send a buffer greater than 1504 bytes (including IP
and MAC headers), the packet will be silently dropped.

As another consequence, the commonly used tool `nc` cannot be used to stress
Wi-SUN networks with a continuous data stream, as `nc` sends 16 kB-long UDP
when fed an endless source such as `/dev/urandom`. There is no option to reduce
frame size (or to enable `IPV6_DONTFRAG`).

Therefore, sending UDP packets with `IPV6_DONTFRAG` is recommended. Use
`IPV6_PATHMTU` and `IPV6_RECVPATHMTU` to determine the optimal packet size.
Read [RFC 8900][13] for more insights on the question.

[12]: https://www.rfc-editor.org/rfc/rfc8200.html#section-5
[13]: https://www.rfc-editor.org/rfc/rfc8900.html

<br clear="right"/><!-- Right align the Wi-SUN Logo -->

# Wi-SUN br & node simulation on Linux host

The compilation also generate binary `wshwsim` that simulate a RF device.
This binary is not installed by default, you will find it in your build
directory.

In wshwsim main code we use `openpty()` to create a pseudo UART device (a PTY).
When start wshwsim deamon by:

    wshwsim /tmp/rcp_socket /tmp/rf_driver

    ./wsbrd -A pki/ca_cert.pem -C pki/br_cert.pem -K pki/br_key.pem -u /dev/pty2

The project also generate binary `wsnode` that act as a Wi-SUN node. `wsnode`
will connect automatically to the Wi-SUN network and reply to the ping requests.
It is sufficient for rough testing. Like `wsbrd`, `wsnode` need a RF device to
work. The hardware can also been simulated with `wshwsim`:

Finally, you can link the `wshwsim` together using a third PTY:

```bash
###################################################################################
# Wi-SUN br & node simulation on Linux host
# 在Linux host上可以同时运行: wsbrd + wshwsim 和 wshwsim + wsnode。 
# 前者是Wi-SUN BR包含从MAC sublayer到网络层，后者是完整的Wi-SUN node。
# 上面的"+"反映的是：两个host上运行的线程间通过伪终端(PeudoTerminal)相联系，
# 与现实中NCP到Linux host的串口/dev/ttyACMx等效。
# 两个模拟分别与BR和Node相连的NCP的wshwsim的RF端通过socket相连，模拟空中channel。
#
#                         tun0
#---------------------------------------------------------------------------------
#                   | application     |                    | application     |
#                   | RPL/ICMPv6/IPv6 |                    | RPL/ICMPv6/IPv6 |
#                   | 6LowPAN         |                    | 6LowPAN         |
#   wsbrd:          | LLC             |            wsnode: | LLC             | 
#---------------------------------------------------------------------------------
#                           |                                        |
#   PeudoTerminal       /dev/pts/m                              /dev/pts/n
#                           |                                        |
#---------------------------------------------------------------------------------
#   wshwsim:br      | MAC sublayer    |      wshwsim:node   | MAC sublayer   |
#---------------------------------------------------------------------------------
#                           |                                        |
#  wssimserver:             |____________socket_rf_driver____________|
#
###################################################################################

# run the following in sequence:  
wssimserver /tmp/rf_driver
wshwsim /tmp/rcp_socket /tmp/rf_driver -T rf,hdlc,hif
sudo wsbrd -F examples/wsbrd.conf -u /dev/pts/2 -T 15.4-mngt,15.4,eap
wshwsim /tmp/rcp_node_socket /tmp/rf_driver -T rf,hdlc,hif
sudo wsnode -F examples/wsnode.conf -u /dev/pts/8 -T 15.4-mngt,15.4,eap
# each time the number /dev/pts/# is different depend on wshwsim
```

# Set up wisun-br-linux + RCP on RaspberryPi4B

 * burn a clean RaspIOS(desktop) on a TF card (>32G), using RaspberryPi imager 
 * insert TF card into Pi4B start finshing remaining set up
 * open a terminal on RaspPiIOS, Generating a new SSH key:
```bash
ssh-keygen -t ed25519 -C "rong72@yeah.net"
cat .ssh/id_ed25519.pub 
# then copy the crypto string to your github profile
```
 * update/upgrade and install essencial tools on RaspPiIOS as below:
```bash
sudo apt update -y; sudo apt upgrade -y
sudo apt install git net-tools ssh traceroute g++ lm-sensors tcpdump -y
sudo apt install libnl-3-dev libnl-route-3-dev libcap-dev libpcap-dev -y
sudo apt install libsystemd-dev libdbus-1-dev cargo cmake ninja-build pkg-config lrzsz -y
``` 
 * enable ipv6 forward on RaspPi4B
```bash
# edit /etc/sysctl.conf as the following 2 lines
# net.ipv4.ip_forward=1
# net.ipv6.conf.all.forwarding=1
sudo nano /etc/sysctl.conf
# activate the changes immedialtely
sudo sysctl -p
```
 * get and compile mbedtls (currently v3.0.0)
```bash
cd ~
mkdir Git_repository
cd Git_repository
# clone, build and install mbedtls
git clone --branch=v3.0.0 https://github.com/ARMmbed/mbedtls
cd mbedtls
cmake -G Ninja .
ninja
sudo ninja install
```
 * get, build and install wisun-br-linux
```bash
# clone, build and install wisun-linux-br
cd ~/Git_repository
git clone git@github.com:rongjun72/wisun-br-linux.git
cd wisun-br-linux
cmake -G Ninja .
ninja
sudo ninja install
```