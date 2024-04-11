LOG_PATH=$(pwd)/logs
WisunBR_PATH="~/Git_repository/wisun-br-rong"
TBC_usr=$(whoami)
TBC_ip="192.168.31.179"

# Device Under Test(DUT) are border router(BR) implemented by RaspberryPi+RCP
# witch can be controlled through ssh remote access  
BRRPI_usr="cmt"
BRRPI_ip="192.168.31.97"
BRRPI_mac="8e:1f:64:5e:40:00:f2:0b"
BRRPI_ipv6="fd00:6868:6868:0:8c1f:645e:4000:f20b"
BRRPI_path="/home/cmt/Git_repository/wisun-br-rong"
silabspti=/home/${TBC_usr}/Git_repository/java_packet_trace_library/silabs-pti/build/libs/silabs-pti-1.12.2.jar
# Jlink serial number of TBUs
wsnode0_sn=440088192; wsnode0_mac="8c:1f:64:5e:40:bb:00:01"
wsnode05_sn=440271870; wsnode05_mac="8c:1f:64:5e:40:bb:00:04"
wsnode1_sn=440054721; wsnode1_mac="8c:1f:64:5e:40:bb:00:02"
wsnode2_sn=440136634; wsnode2_mac="8c:1f:64:5e:40:bb:00:03"

# Test Bed Units(TBU) connected to Test Bed through serial port
# check serial port discripter for your test bed
# then get access permission to serial ports
wsnode0=/dev/ttyACM0; sudo chmod 777 $wsnode0
wsnode05=/dev/ttyACM3; sudo chmod 777 $wsnode05
wsnode1=/dev/ttyACM1; sudo chmod 777 $wsnode1
wsnode2=/dev/ttyACM2; sudo chmod 777 $wsnode2

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
#-------------------------------------------------------------

# -------------------------------------------------------------------------------------------------
# Trickle Timer Settings
# -------------------------------------------------------------------------------------------------
# Name                              Description                         Range	    Value
# DEFAULT_DIO_INTERVAL_DOUBLINGS    The DIO trickle timer Imax value.   1-8         2
#                                   Small network - 2 (an Imax of       doublings
#                                   approximately 128 seconds)	
# DEFAULT_DIO_INTERVAL_MIN 	        The DIO trickle timer Imin value    1-255       15 
#                                   is defined as 2 to the power of 
#                                   this value in millseconds.
#                                   Small network - 15 (an Imin of 
#                                   approximtety 32 seconds)	
# DISC_IMIN                         Discovery Trickle Timer Imin        1-255       15
#                                   Small network - 15 second
# DISC_IMAX                         Discovery Trickle timer Imax        1-8         2
#                                   Small network - 2 (max Trickle      doublings
#                                   interval = approximately 60 sec).   
# -------------------------------------------------------------------------------------------------
DEFAULT_DIO_INTERVAL_DOUBLINGS=2
DEFAULT_DIO_INTERVAL_MIN=15
DISC_IMIN=15
DISC_IMAX=2


source br_cert_funcions.sh

# open a minicom window to display serial port echo
gnome-terminal --window --title="node0" --geometry=80x25+200+100 \
                                         -e 'minicom -D /dev/ttyACM0 -b 115200 -8' \
                  --tab --title="node05" -e 'minicom -D /dev/ttyACM3 -b 115200 -8' \
                  --tab --title="node1"  -e 'minicom -D /dev/ttyACM1 -b 115200 -8' \
                  --tab --title="node2"  -e 'minicom -D /dev/ttyACM2 -b 115200 -8'
stty -F ${wsnode0}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 100
stty -F ${wsnode05} ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 100
stty -F ${wsnode1}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 100
stty -F ${wsnode2}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 100

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
wsnode0_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode0_sn)
echo "find wsnode0_sn:$wsnode0_sn and wsnode0_netif: $wsnode0_netif"
wsnode05_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode05_sn)
echo "find wsnode05_sn:$wsnode05_sn and wsnode05_netif: $wsnode05_netif"
wsnode1_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode1_sn)
echo "find wsnode1_sn:$wsnode1_sn and wsnode1_netif: $wsnode1_netif"
wsnode2_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode2_sn)
echo "find wsnode2_sn:$wsnode2_sn and wsnode2_netif: $wsnode2_netif"
# delete temprary file
rm -f si_pti_discover.txt



# test begin....................
#source test_PAN-PA-SELECT-2.sh

#source test_PAN-KEY-TLS-2.sh

#source test_PAN-KEY-TLS-4.sh

#source test_PAN-KEY-TLS-6.sh

#source test_PAN-KEY-TLS-8.sh

#source test_PAN-KEY-TLS-11.sh

#source test_ROLL-RPL-ROOT-1.sh

#source test_DHCPv6-SERVER.sh

#source test_6LO-ND-LBR-1.sh

#source test_DIRECT-HASH-HOP-LBR-1.sh

#source test_DIRECT-SHORT-DWELL-LBR-1.sh

source test_DIRECT-MIXED-DWELL-LBR-1.sh

#source test_DIRECT-EXC-CHAN-SEND-LBR-1.sh



sudo rm -f ${LOG_PATH}/sed*.*
# check session id of serial port and wsbrd(ssh RPi) and kill them
node0_id=$(ps -u | grep 'minicom -D' | grep $wsnode0 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
node05_id=$(ps -u | grep 'minicom -D' | grep $wsnode05 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
node1_id=$(ps -u | grep 'minicom -D' | grep $wsnode1 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
node2_id=$(ps -u | grep 'minicom -D' | grep $wsnode2 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
echo "kill minicom serial port 0: $node0_id"; kill $node0_id;
echo "kill minicom serial port 0: $node05_id"; kill $node05_id;
echo "kill minicom serial port 1: $node1_id"; kill $node1_id;
echo "kill minicom serial port 2: $node2_id"; kill $node2_id;
