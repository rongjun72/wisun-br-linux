LOG_PATH=$(pwd)/log
WisunBR_PATH="~/Git_repository/wisun-br-rong"

# Device Under Test(DUT) are border router(BR) implemented by RaspberryPi+RCP
# witch can be controlled through ssh remote access  
BRRPI_usr="cmt"
BRRPI_ip="192.168.31.97"
BRRPI_path="/home/cmt/Git_repository/wisun-br-rong"
silabspti=/home/jurong/Git_repository/java_packet_trace_library/silabs-pti/build/libs/silabs-pti-1.12.2.jar
# Jlink serial number of TBUs
wsnode0_sn=440088192
wsnode1_sn=440054721
wsnode2_sn=440136634
wsnode0_mac="8c:1f:64:5e:40:bb:00:01"
wsnode1_mac="8c:1f:64:5e:40:bb:00:02"
wsnode2_mac="8c:1f:64:5e:40:bb:00:03"

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
#nohup minicom -D $wsnode0 &
# serial port config
# gnome-terminal --tab
stty -F ${wsnode0} ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 100
#gnome-terminal --window -- cat ${wsnode0};

# --------------------------------------------------------------------------------------------
# Silabs Wi-SUN node packet capture
# Hardware aspect, Silabs has PTI(Packet Tracking Interface) support which 
# send network packets over debug channel (ethernet adapter)
# Sofeware aspect, the java project: silabs-pti-X.Y.Z.jar communicate with Silabs WSTK adapters 
# and stream debug channel messages to a file.
# Data can be saved in a PCAPNG format to be consumed by wireshark, Silicon Labs Network Analyzer
# , or as plain text or binary files.
# --------------------------------------------------------------------------------------------
# check the pti adapter network interface of the wsnodes
java -jar $silabspti -discover > si_pti_discover.txt
line_sn=$(sed -n '/'$wsnode0_sn'/=' si_pti_discover.txt)
if [ -n "$line_sn" ]; then
  echo "node 0 jlink sn found in line: $line_sn..."
  line_ip=$(($line_sn + 1)); 
  wsnode0_netif=$(sed -n "${line_ip}p" si_pti_discover.txt | sed 's/^.*netif=//g')
  echo "node 0 net if is: $wsnode0_netif"
fi
line_sn=$(sed -n '/'$wsnode1_sn'/=' si_pti_discover.txt)
if [ -n "$line_sn" ]; then
  echo "node 1 jlink sn found in line: $line_sn..."
  line_ip=$(($line_sn + 1)); 
  wsnode1_netif=$(sed -n "${line_ip}p" si_pti_discover.txt | sed 's/^.*netif=//g')
  echo "node 1 net if is: $wsnode1_netif"
fi
line_sn=$(sed -n '/'$wsnode2_sn'/=' si_pti_discover.txt)
if [ -n "$line_sn" ]; then
  echo "node 0 jlink sn found in line: $line_sn..."
  line_ip=$(($line_sn + 1)); 
  wsnode2_netif=$(sed -n "${line_ip}p" si_pti_discover.txt | sed 's/^.*netif=//g')
  echo "node 2 net if is: $wsnode2_netif"
fi

rm -f si_pti_discover.txt


#modulation_rate=(50 150)

# The GTKs for PAN A are set to a specific set of values when the BR is a TBU
# -----------------------------------------------------------------------------
# gtk[0] = BB:06:08:57:2C:E1:4D:7B:A2:D1:55:49:9C:C8:51:9B
#    gak : A1 41 66 C2 5B B3 F8 F6 C9 03 C4 F9 83 88 68 80
#    gak1: A14166C25BB3F8F6C903C4F983886880
# gtk[1] = 18:49:83:5A:01:68:4F:C8:AC:A5:83:F3:70:40:F7:4C
#	   gak : C2 15 70 6C BC 81 21 9D 98 7C 6E 4A EE 1C C2 A0
#	   gak2: C215706CBC81219D987C6E4AEE1CC2A0
# gtk[2] = 59:EA:58:A4:B8:83:49:38:AD:CB:6B:E3:88:C2:62:63
#	   gak : 92 62 9A 53 29 F5 4C 16 A6 7D C7 5A 3F B9 22 A9
#	   gak3: 92629A5329F54C16A67DC75A3FB922A9
# gtk[3] = E4:26:B4:91:BC:05:4A:F3:9B:59:F0:53:EC:12:8E:5F
#	   gak : 96 06 14 33 9D 94 40 5A 4A 4A 44 2D 71 3E 3B 44
#	   gak4: 960614339D94405A4A4A442D713E3B44
# ---------------------------------------------------------
# GAKs when net name set to "Wi-SUN test"
# GAK[0]: a7 6c 4a e5 f6 41 95 d0 40 d7 e6 91 37 61 03 6e
# GAK[1]: 49 a7 17 1a b3 0f 69 66 ff d5 b9 e3 5e e0 4c 44
# GAK[2]: 76 fb e0 62 c4 0b 28 61 43 66 fd 17 5f 4b 46 7c
# GAK[3]: 58 03 65 9d db 4a 6d 4e 53 1f a4 30 6b 57 de cf
# ----------------------------------------
# GAK[0]: a76c4ae5f64195d040d7e6913761036e
# GAK[1]: 49a7171ab30f6966ffd5b9e35ee04c44
# GAK[2]: 76fbe062c40b28614366fd175f4b467c
# GAK[3]: 5803659ddb4a6d4e531fa4306b57decf
# ---------------------------------------------------------
# the GTKs for PAN B are set to a specific set of values when the BR is a TBU:
# -----------------------------------------------------------------------------
# gtk[0] = 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
#    gak : 9E 72 51 81 82 20 A6 52 13 1D 6C 22 48 E4 36 44
# gtk[1] = 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22
#    gak : 08 86 E3 2E 85 80 A5 85 B9 09 82 9D 19 5E 3F 08
# gtk[2] = 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33
#    gak : 26 7A ED 2C 20 41 0B 3D 5F B9 AE 23 60 D5 F0 4E
# gtk[3] = 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44
#    gak : CD 46 A0 50 AF 2B F1 6C F5 A0 E3 68 E5 10 44 9C
#Note: the given GAKs are for Net Name of “WiSUN PAN”.

function ssh_command
{
  ssh ${BRRPI_usr}@${BRRPI_ip} "$1"
}

function wisun_set
{
  seria_port=$1
  echo "wisun reset" > $seria_port; sleep 0.5;
  echo "wisun set wisun.network_name \"WiSUN test\"" > $seria_port
  echo "wisun set wisun.regulatory_domain NA" > $seria_port 
  echo "wisun set wisun.operating_class 3" > $seria_port
  echo "wisun set wisun.operating_mode 5" > $seria_port
  echo "wisun set wisun.unicast_dwell_interval 15" > $seria_port
  echo "wisun set wisun.allowed_channels 0-255" > $seria_port
  # clear the white list before set the list
  echo "wisun mac_allow FF:FF:FF:FF:FF:FF:FF:FF" > $seria_port
  echo "wisun save" > $seria_port
  sleep 0.5
}

function join_fan10
{
  seria_port=$1
  echo "wisun join_fan10" > $seria_port
}

function ssh_check_and_kill_wsbrd
{
  BRRPI_usr=$1
  BRRPI_ip=$2

  # check the thread number of wsbrd
  # return of "ps -u wsbrd" is "PID TTY TIME CMD $pid_val ? hour:min:sec wsbrd"
  # here we extact the thread id : $pid_val, if existed, kill it
  thread=$(echo $(ssh ${BRRPI_usr}@${BRRPI_ip} "ps -u wsbrd") | sed 's/^.*CMD //g' | sed 's/ .*$//g')
  echo "------check the thread number of wsbrd: $thread"
  # kill existing wsbrd thread before start
  if [ -n "$thread" ]; then
    echo "------kill the existing wsbrd thread: $thread before start" 
    ssh ${BRRPI_usr}@${BRRPI_ip} "sudo kill $thread";
  fi
}

function ssh_start_wsbrd_window
{
  BRRPI_usr=$1
  BRRPI_ip=$2
  wisun_domain=$3
  wisun_mode=$4
  wisun_class=$5
  trace_set="15.4-mngt,15.4,eap,icmp-rf"

  gnome-terminal --window -- ssh ${BRRPI_usr}@${BRRPI_ip} \
    "cd $BRRPI_path;sudo wsbrd -F wsbrd.conf  -d $wisun_domain -m $wisun_mode -c $wisun_class -T $trace_set"
  sleep 1;  
}

# --------------------------------------------------------------------------------------------
# Before start test script, border router RCP packet capture through wireshark should be set.
# USB serial port dongle: TX/RX/GND connect to PD8/PD9/GND on CMT RCP board
# We have serial port assistant ((packet sniffer)) pipes sniffer packet from RCP UART to wireshark
# The tool is application on windows. So, we have to run RCP captrue on windows side which
# is independant from the Linux TBU platform.
#
# Opperation sequence of wireshark piped capture:
#   1. start packet sniffer application (D:\software\sniffer_rev1_2\serial.exe)
#   2. open pipe, and press confirm button in the pop-up box
#   3. start wireshark and add new pipe with name:  \\.\pipe\sniffer_pip
#       in menu:->Capture->Options->Manage Interfaces->pipes  
#       In windows cmd: wireshark -ni \\.\pipe\sniffer_pip
#   4. start pipe capture, and then you can see something poped like "pipe connected" in sniffer app 
#   5. search set and open corresponding serial port in sniffer app
#   ...capture for RCP packet start in wireshark
# --------------------------------------------------------------------------------------------
# border router configurations:
#PAN_ID=35
#network_name="Wi-SUN test"
wisun_domain="NA"
wisun_mode="5"
wisun_class="3"
#-------------------------------------------------------------

# test begin........
#wisun_set $wsnode0
#wisun_set $wsnode1
#wisun_set $wsnode2
#echo "wisun mac_allow $wsnode1_mac" > $wsnode0
#echo "wisun mac_allow $wsnode2_mac" > $wsnode1


# check the thread number of wsbrd
ssh_check_and_kill_wsbrd $BRRPI_usr $BRRPI_ip;

echo "------start wsbrd application on Raspberry Pi..."
wisun_domain="NA"; wisun_mode="5"; wisun_class="3"
ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class

echo "------start wsnode packet capture, for 400s..."
gnome-terminal --window -- \
  sudo java -jar $silabspti -ip=$wsnode0_netif -driftCorrection=disable -time=400000 -format=log -out=test_cap$(date "+%m%d_%H-%M").log

echo "------start wsnode join_fan10-------"
time_0=$(date +%s%N); echo "wisun disconnect" > $wsnode0 
time_1=$(date +%s%N); echo "send disconnect: $((($time_1 - $time_0)/1000000))ms"; echo "wisun join_fan10" > $wsnode0 
time_2=$(date +%s%N); echo "send join_fan10: $((($time_2 - $time_1)/1000000))ms";





for wait_idx in $(seq 0 40)
do
  #echo $(date)
  echo -ne "\r"
  sleep 10
  for idx in $(seq 0 $wait_idx)
  do
    if [ "$idx" = "0" ]; then
      echo -ne "\r."
    else
      echo -n "."
    fi
  done
  for idx in $(seq 0 $((50 - $wait_idx)))
  do
    echo -n "|"
  done
done


