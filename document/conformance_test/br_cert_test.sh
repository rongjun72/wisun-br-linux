LOG_PATH=$(pwd)/log
WisunBR_PATH="~/Git_repository/wisun-br-rong"

# Device Under Test(DUT) are border router(BR) implemented by RaspberryPi+RCP
# witch can be controlled through ssh remote access  
BorderRouterRPi_usr="cmt"
BorderRouterRPi_ip="192.168.31.97"

# Test Bed Units(TBU) connected to Test Bed through serial port
# check serial port discripter for your test bed
wsnode0=/dev/ttyACM0
wsnode1=/dev/ttyACM1
wsnode2=/dev/ttyACM2
# get access permission to serial ports
sudo chmod 777 $wsnode0
sudo chmod 777 $wsnode1
sudo chmod 777 $wsnode2
# might not needed
nohup minicom -D $wsnode0 &
# serial port config
stty -F ${wsnode0} ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 10 | cat ${wsnode0}

#modulation_rate=(50 150)

# The GTKs for PAN A are set to a specific set of values when the BR is a TBU
# -----------------------------------------------------------------------------
# gtk[0] = BB:06:08:57:2C:E1:4D:7B:A2:D1:55:49:9C:C8:51:9B
#    gak : A1:41:66:C2:5B:B3:F8:F6:C9:03:C4:F9:83:88:68:80
# gtk[1] = 18:49:83:5A:01:68:4F:C8:AC:A5:83:F3:70:40:F7:4C
#	   gak : C2:15:70:6C:BC:81:21:9D:98:7C:6E:4A:EE:1C:C2:A0
# gtk[2] = 59:EA:58:A4:B8:83:49:38:AD:CB:6B:E3:88:C2:62:63
#	   gak : 92:62:9A:53:29:F5:4C:16:A6:7D:C7:5A:3F:B9:22:A9
# gtk[3] = E4:26:B4:91:BC:05:4A:F3:9B:59:F0:53:EC:12:8E:5F
#	   gak : 96:06:14:33:9D:94:40:5A:4A:4A:44:2D:71:3E:3B:44
# ---------------------------------------------------------
# the GTKs for PAN B are set to a specific set of values when the BR is a TBU:
# -----------------------------------------------------------------------------
# gtk[0] = 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
#    gak : 9E:72:51:81:82:20:A6:52:13:1D:6C:22:48:E4:36:44
# gtk[1] = 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22
#    gak : 08:86:E3:2E:85:80:A5:85:B9:09:82:9D:19:5E:3F:08
# gtk[2] = 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33
#    gak : 26:7A:ED:2C:20:41:0B:3D:5F:B9:AE:23:60:D5:F0:4E
# gtk[3] = 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44
#    gak : CD:46:A0:50:AF:2B:F1:6C:F5:A0:E3:68:E5:10:44:9C
#Note: the given GAKs are for Net Name of “WiSUN PAN”.

function ssh_command
{
  ssh ${BorderRouterRPi_usr}@${BorderRouterRPi_ip} "$1"
}

function wisun_set
{
  seria_port=$1
  echo "wisun reset" > $seria_port
  echo "wisun set wisun.network_name \"WiSUN PAN\"" > $seria_port
  echo "wisun set wisun.regulatory_domain NA" > $seria_port 
  echo "wisun set wisun.operating_class 3" > $seria_port
  echo "wisun set wisun.operating_mode 5" > $seria_port
  echo "wisun set wisun.unicast_dwell_interval 15" > $seria_port
  echo "wisun set wisun.allowed_channels 0-255" > $seria_port
  echo "wisun save" > $seria_port
}

function join_fan10
{
  seria_port=$1
  echo "wisun join_fan10" > $seria_port
}

echo "------start wsbrd-------"
# check the thread number of wsbrd
thread=$(echo $(ssh_command "ps -u wsbrd") | sed 's/?.*//' | sed 's/[^0-9/]*//g')
echo "---check the thread number of wsbrd: $thread"
# kill existing wsbrd thread before start
if [ -n "$thread" ]; then
  echo "---kill the existing wsbrd thread: $thread before start" 
  ssh_command "sudo kill $thread";
fi
echo "---start wsbrd application on Raspberry Pi..."
ssh_command "cd ~/Git_repository/wisun-br-rong;sudo wsbrd -F wsbrd.conf -T 15.4-mngt,15.4,eap,icmp-rf,icmp-tun >> working.log" &

echo "------start wsnode-------"
wisun_set $wsnode0
join_fan10 $wsnode0


