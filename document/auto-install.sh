# Generating a new SSH key
# ssh-keygen -t ed25519 -C "rong72@yeah.net"
# cat .ssh/id_ed25519.pub 

#
#
sudo apt update -y; sudo apt upgrade -y
sudo apt install git net-tools ssh traceroute g++ lm-sensors tcpdump -y

cd ~/Documents
#wget --content-disposition https://az764295.vo.msecnd.net/stable/660393deaaa6d1996740ff4880f1bad43768c814/code_1.80.0-1688477953_arm64.deb
sudo dpkg -i code_1.80.0-1688477953_arm64.deb
rm -rf code_1.80.0-1688479026_amd64.deb

sudo apt install libnl-3-dev libnl-route-3-dev libcap-dev libpcap-dev -y
sudo apt install libsystemd-dev libdbus-1-dev cargo cmake ninja-build pkg-config lrzsz -y

sudo nano /etc/sysctl.conf
# net.ipv4.ip_forward=1
# net.ipv6.conf.all.forwarding=1
sudo sysctl -p

cd ~
mkdir Git_repository
cd Git_repository
# clone, build and install mbedtls
git clone --branch=v3.0.0 https://github.com/ARMmbed/mbedtls
cd mbedtls
cmake -G Ninja .
ninja
sudo ninja install

sudo nano .cargo/config
#    # `$HOME/.cargo/config` 
#    [source.crates-io]
#    #registry = "https://github.com/rust-lang/crates.io-index"
#    
#    # 
#    replace-with = 'ustc'
#    
#    # 
#    [source.ustc]
#    registry = "git://mirrors.ustc.edu.cn/crates.io-index"

nano examples/wsbrd.conf
# ipv6_prefix = fd00:6868:6868:0::/64
# neighbor_proxy=eth0


# clone, build and install wisun-linux-br
cd ~/Git_repository
git clone git@github.com:rongjun72/wisun-br-linux.git
cd wisun-br-linux
cmake -G Ninja .
ninja
sudo ninja install


sudo wsbrd -F wsbrd.conf -T eap,15.4,15.4-mngt,dhcp