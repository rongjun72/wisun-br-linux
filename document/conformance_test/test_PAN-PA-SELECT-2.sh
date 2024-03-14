source br_cert_funcions.sh
# ------------- global variables begin ----------------------------------
# DO NOT change the global vars in this file
# -----------------------------------------------------------------------
# $LOG_PATH:      location to generate test logs
# $BRRPI_usr:     usr name of RaspberryPi which runing wsbrd
# $BRRPI_ip:      ip address of RaspberryPi which runing wsbrd
# $BRRPI_path:    location of wsbrd excutable in RaspberryPi
# $TBC_usr:       usr name of TestBedController Ubuntu computer
# $TBC_ip:        ip address of TestBedController Ubuntu computer
# $silabspti:     location of silabs-pti-x.x.jar in TestBedController
# $wsnode0:       serial port descripter for wisun node 0, rank=1
# $wsnode1:       serial port descripter for wisun node 1, rank=2
# $wsnode2:       serial port descripter for wisun node 2, rank=3
# $wsnode0_netif: network interface of WSTK0 used for PTI capture
# $wsnode1_netif: network interface of WSTK1 used for PTI capture
# $wsnode2_netif: network interface of WSTK2 used for PTI capture
# ------------- global variables end ------------------------------------
TEST_TIME=$(date "+%m%d_%H-%M");
node0_pti_cap_file=${LOG_PATH}/Node0Cap_PAN-PA-SELECT-2_${TEST_TIME}.log
wsbrd_cap_file=${LOG_PATH}/BrCap_PAN-PA-SELECT-2_${TEST_TIME}.pcapng

# Wi-SUN network configurations:
# ------------- test bed configurations begin ---------------------------
#PAN_ID=35
#network_name="Wi-SUN test"
wisun_domain="NA"; wisun_mode="0x1b"; wisun_class="1"
wisun_domain="NA"; wisun_mode="5"; wisun_class="3"
# ------------- test bed configurations end ------------------------------


# TBUs config setting........
wisun_node_set $wsnode0 $wisun_domain $wisun_mode $wisun_class
wisun_node_set $wsnode1 $wisun_domain $wisun_mode $wisun_class
wisun_node_set $wsnode2 $wisun_domain $wisun_mode $wisun_class
#echo "wisun mac_allow $wsnode1_mac" > $wsnode0
#echo "wisun mac_allow $wsnode2_mac" > $wsnode1

# check the thread number of wsbrd
ssh_check_and_kill_wsbrd $BRRPI_usr $BRRPI_ip;

# get/modify/overwrite the wsbrd.conf file before start wsbrd application in RPi
scp ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf ${LOG_PATH}/wsbrd.conf
wisun_br_config ${LOG_PATH}/wsbrd.conf $wisun_domain $wisun_mode $wisun_class
scp ${LOG_PATH}/wsbrd.conf ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf
rm -f ${LOG_PATH}/wsbrd.conf

echo "------start wsbrd application on Raspberry Pi..."
ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class


capture_time=200
echo "------start wsnode packet capture, for ${capture_time}s..."
gnome-terminal --window --title="Node Capture" --geometry=90x24+200+0 -- \
  sudo java -jar $silabspti -ip=$wsnode0_netif -driftCorrection=disable -time=$(($capture_time*1000)) -format=log -out=${node0_pti_cap_file}

echo "------start wsnode join_fan10-------"
time_0=$(date +%s%N); echo "wisun disconnect" > $wsnode0 
time_1=$(date +%s%N); echo "send disconnect: $((($time_1 - $time_0)/1000000))ms"; echo "wisun join_fan10" > $wsnode0 
time_2=$(date +%s%N); echo "send join_fan10: $((($time_2 - $time_1)/1000000))ms";





display_wait_progress $(($capture_time/10));
# check session id of serial port and wsbrd(ssh RPi) and kill them
node0_id=$(ps -u | grep 'minicom -D' | grep $wsnode0 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
node1_id=$(ps -u | grep 'minicom -D' | grep $wsnode1 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
node2_id=$(ps -u | grep 'minicom -D' | grep $wsnode2 | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
wsbrd_id=$(ps -u | grep cd | grep 'sudo wsbrd -F' | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
echo "kill minicom serial port 0: $node0_id"; kill $node0_id;
echo "kill minicom serial port 1: $node1_id"; kill $node1_id;
echo "kill minicom serial port 2: $node2_id"; kill $node2_id;
echo "kill wsbrd window: $wsbrd_id, actually wsbrd is still running on remote RPi"; kill $wsbrd_id;
# -------------------------------------------------------------------------------------------------
# copy border router host/rcp received message pcapng file from RapspberryPi
# prerequiste is uncomment "pcap_file = /tmp/wisun_dump.pcapng" in wsbrd.conf in RPi
# -------------------------------------------------------------------------------------------------
scp ${BRRPI_usr}@${BRRPI_ip}:/tmp/wisun_dump.pcapng ${wsbrd_cap_file}






# -------------------------------------------------------------------------------------------------
# ------- post test analysis begin based on ${wsbrd_cap_file} and ${node0_pti_cap_file}
# -------------------------------------------------------------------------------------------------