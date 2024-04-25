export LOG_PATH=$(pwd)/logs
export WisunBR_PATH="~/Git_repository/wisun-br-rong"
export TBC_usr=$(whoami)
export TBC_ip="192.168.31.179"

# Device Under Test(DUT) are border router(BR) implemented by RaspberryPi+RCP
# witch can be controlled through ssh remote access  
export BRRPI_usr="cmt"
export BRRPI_ip="192.168.31.97"
export BRRPI_mac="8e:1f:64:5e:40:00:f2:0b"
export BRRPI_lladdr="fe80::8c1f:645e:4000:f20b"
export BRRPI_ipv6="fd00:6868:6868:0:8c1f:645e:4000:f20b"
export BRRPI_path="/home/cmt/Git_repository/wisun-br-rong"
export silabspti=/home/${TBC_usr}/Git_repository/java_packet_trace_library/silabs-pti/build/libs/silabs-pti-1.12.2.jar
# Jlink serial number of TBUs
export wsnode0_sn=440088192;    
export wsnode05_sn=440045706;   
export wsnode1_sn=440054721;    
export wsnode2_sn=440136634;    
export wsnode0_mac="8c:1f:64:5e:40:bb:00:01"
export wsnode05_mac="8c:1f:64:5e:40:bb:00:04"
export wsnode1_mac="8c:1f:64:5e:40:bb:00:02"
export wsnode2_mac="8c:1f:64:5e:40:bb:00:03"

export wsnode0_ipaddr="fd00:6868:6868:0:8e1f:645e:40bb:1"
export wsnode05_ipaddr="fd00:6868:6868:0:8e1f:645e:40bb:4"
export wsnode1_ipaddr="fd00:6868:6868:0:8e1f:645e:40bb:2"
export wsnode2_ipaddr="fd00:6868:6868:0:8e1f:645e:40bb:3"
export wsnode0_lladdr="fe80::8e1f:645e:40bb:1"
export wsnode05_lladdr="fe80::8e1f:645e:40bb:4"
export wsnode1_lladdr="fe80::8e1f:645e:40bb:2"
export wsnode2_lladdr="fe80::8e1f:645e:40bb:3"

# Test Bed Units(TBU) connected to Test Bed through serial port
# check serial port discripter for your test bed
# then get access permission to serial ports
export wsnode0=/dev/ttyACM1;    sudo chmod 777 $wsnode0
export wsnode05=/dev/ttyACM0;   sudo chmod 777 $wsnode05
export wsnode1=/dev/ttyACM2;    sudo chmod 777 $wsnode1
export wsnode2=/dev/ttyACM3;    sudo chmod 777 $wsnode2

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
export DEFAULT_DIO_INTERVAL_DOUBLINGS=2
export DEFAULT_DIO_INTERVAL_MIN=15
export DISC_IMIN=15
export DISC_IMAX=2

source br_cert_funcions.sh

rm -f ${LOG_PATH}/wsnode0_serial.log; rm -f ${LOG_PATH}/wsnode05_serial.log
rm -f ${LOG_PATH}/wsnode1_serial.log; rm -f ${LOG_PATH}/wsnode2_serial.log

# open a minicom window to display serial port echo
gnome-terminal --window --title="node0" --geometry=80x25+200+100 \
                                         -e "minicom -D ${wsnode0}  -b 115200 -8 -C ${LOG_PATH}/wsnode0_serial.log" \
                  --tab --title="node05" -e "minicom -D ${wsnode05} -b 115200 -8 -C ${LOG_PATH}/wsnode05_serial.log" \
                  --tab --title="node1"  -e "minicom -D ${wsnode1}  -b 115200 -8 -C ${LOG_PATH}/wsnode1_serial.log" \
                  --tab --title="node2"  -e "minicom -D ${wsnode2}  -b 115200 -8 -C ${LOG_PATH}/wsnode2_serial.log"
stty -F ${wsnode0}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 10
stty -F ${wsnode05} ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 10
stty -F ${wsnode1}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 10
stty -F ${wsnode2}  ispeed 115200 ospeed 115200 -parenb cs8 -cstopb -icanon min 0 time 10

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
export wsnode0_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode0_sn)
export wsnode05_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode05_sn)
export wsnode1_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode1_sn)
export wsnode2_netif=$(find_netif_for_pti_capture si_pti_discover.txt $wsnode2_sn)
echo "find wsnode0_sn:$wsnode0_sn and wsnode0_netif: $wsnode0_netif"
echo "find wsnode05_sn:$wsnode05_sn and wsnode05_netif: $wsnode05_netif"
echo "find wsnode1_sn:$wsnode1_sn and wsnode1_netif: $wsnode1_netif"
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

#source test_DIRECT-MIXED-DWELL-LBR-1.sh

#source test_DIRECT-EXC-CHAN-SEND-LBR-1.sh

#source test_DIRECT-EXC-CHAN-SEND-LBR-2.sh

#source test_DIRECT-EXC-CHAN-LISTEN-LBR-1.sh

#source test_TR51-LISTEN-LBR-1.sh

#source test_UNICAST-DST-DFE-LBR-1.sh

#source test_UNICAST-FWD-DFE-LBR-1.sh

#source test_MULTICAST-ORIGINATOR-LBR-1.sh


sudo rm -f ${LOG_PATH}/sed*.*
# check session id of serial port and wsbrd(ssh RPi) and kill them
#kill_minicoms $wsnode0 $wsnode05 $wsnode1 $wsnode2
