source br_cert_funcions.sh
# -----------------------------------------------------------------------
# Test case :   Border Router as DUT [UNICAST-FWD-DFE-LBR-1]
# -----------------------------------------------------------------------
# Description:  
# This test case verifies a DUT as UDP and ICMPv6 Forwarder.
# 
# We can not set 6lowpan MTU value for Silabs wisun node!!!
# We can test fragmentization by sending ping packet larger than 
# default MTU 1280-byte
# -----------------------------------------------------------------------
# Test Bed records the traffic between the DUT and neighboring test bed 
# devices by subscribing to frames from Test bed devices A-D, Test bed 
# devices E-H and Test bed devices I-L.  
# Here is the configuration for this test case:
#   a. LBR DUT configured for PAN A 
#   b. Test bed Device A, Device B,  Device C and Device D at Rank 1
#   c. Test Bed Device E, Device F, Device G and Device H at Rank 2 
#   d. Test Bed Device I, Device J, Device K and Device L at Rank 3
# -----------------------------------------------------------------------
TEST_CASE_NAME="UNICAST-FWD-DFE-LBR-1"

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
# $wsnode0_mac:   device mac address for wisun node 0, rank=1
# $wsnode1_mac:   device mac address for wisun node 1, rank=2
# $wsnode2_mac:   device mac address for wisun node 2, rank=3
# $wsnode0_netif: network interface of WSTK0 used for PTI capture
# $wsnode1_netif: network interface of WSTK1 used for PTI capture
# $wsnode2_netif: network interface of WSTK2 used for PTI capture
#
# DEFAULT_DIO_INTERVAL_DOUBLINGS=2
# DEFAULT_DIO_INTERVAL_MIN=15
# DISC_IMIN=15
# DISC_IMAX=2
# ------------- global variables end ------------------------------------
debug_option="nDEBUG_ANALYSIS";
if [ "$debug_option" = "DEBUG_ANALYSIS" ]; then
    TEST_TIME="0417_11-08";
else
    TEST_TIME=$(date "+%m%d_%H-%M");
fi

time_start_test=$(($(date +%s%N)/1000000));
nodes_pti_cap_file=${LOG_PATH}/NodesCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
wsbrd_cap_file=${LOG_PATH}/BrCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
NodeCsvFile=${LOG_PATH}/output_node.csv;
BRcsvFile=${LOG_PATH}/output_br.csv
total_time_offset_avg=0;
time_checked=0;
packet_checked="";
step0_time=200; step1_time=200; step2_time=400; step3_time=200


echo "----TEST [$TEST_CASE_NAME] start ..................................................................."
if [ "$debug_option" = "DEBUG_ANALYSIS" ]; then
    echo "skip TBUs and DUTs configurations..."
else
    # Wi-SUN network configurations:
    # ------------- test bed configurations begin ---------------------------
    #PAN_ID=35
    #network_name="Wi-SUN test"
    wisun_domain="NA"; wisun_mode="0x1b"; wisun_class="1"
    wisun_domain="NA"; wisun_mode="5"; wisun_class="3"
    # ------------- test bed configurations end ------------------------------
    echo "----------------------------Test Configuration------------------------------"
    echo "Node 0 wisun_domain: $wisun_domain"
    echo "Node 0 wisun_mode:   $wisun_mode"
    echo "Node 0 wisun_class:  $wisun_class"
    echo "-----------------------------------------------------------------------------"

    # TBUs config setting........
    # Currently, we can not change channel function of Silabs wisun node...
    WISUN_NODE0_CONFIG=("disconnect" "yes" "domain" $wisun_domain "mode" $wisun_mode "class" $wisun_class 
        "unicast_dwell_interval" 15 "allowed_channels" "0" "allowed_mac64" $BRRPI_mac "allowed_mac64" $wsnode1_mac);
    WISUN_NODE05_CONFIG=("disconnect" "yes" "domain" $wisun_domain "mode" $wisun_mode "class" $wisun_class 
        "unicast_dwell_interval" 255 "allowed_channels" "0" "allowed_mac64" $BRRPI_mac "allowed_mac64" $wsnode1_mac);
    WISUN_NODE1_CONFIG=("disconnect" "yes" "domain" $wisun_domain "mode" $wisun_mode "class" $wisun_class 
        "unicast_dwell_interval" 15 "allowed_channels" "0" "allowed_mac64" $wsnode0_mac "allowed_mac64" $wsnode2_mac);
    WISUN_NODE2_CONFIG=("disconnect" "yes" "domain" $wisun_domain "mode" $wisun_mode "class" $wisun_class 
        "unicast_dwell_interval" 15 "allowed_channels" "0" "allowed_mac64" $wsnode1_mac);
    wisun_node_set_new $wsnode0 WISUN_NODE0_CONFIG
    wisun_node_set_new $wsnode05 WISUN_NODE05_CONFIG
    wisun_node_set_new $wsnode1 WISUN_NODE1_CONFIG
    wisun_node_set_new $wsnode2 WISUN_NODE2_CONFIG

    # check the thread number of wsbrd
    ssh_check_and_kill_wsbrd $BRRPI_usr $BRRPI_ip;

    # get/modify/overwrite the wsbrd.conf file before start wsbrd application in RPi
    scp ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf ${LOG_PATH}/wsbrd.conf
    WISUN_BR_CONFIG=("domain" $wisun_domain "mode" $wisun_mode "class" $wisun_class 
    "unicast_dwell_interval" 15 "allowed_channels" "0" "allowed_mac64--" $wsnode0_mac "allowed_mac64--" $wsnode05_mac);
    wisun_br_config_new ${LOG_PATH}/wsbrd.conf WISUN_BR_CONFIG
    scp ${LOG_PATH}/wsbrd.conf ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf
    rm -f ${LOG_PATH}/wsbrd.conf

    echo "----------------------------start wsbrd application on Raspberry Pi...-------"
    TIME_START_WSBRD=$(($(date +%s%N)/1000000 - $time_start_test)); # uint in ms
    TIME_START_WSBRD=$(echo "$TIME_START_WSBRD / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
    echo "start wsbrd at time: $TIME_START_WSBRD"
    ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class
    #sleep 5
    #  We can not set 6lowpan MTU for Silabs' Wi-SUN node, we may neglect setting below
    #ssh ${BRRPI_usr}@${BRRPI_ip} "sudo wsbrd_cli set-icmpv6-mtu-size 700";
    sleep 20

    capture_time=$(($step0_time + $step1_time + $step2_time + $step3_time))
    echo "----------------------------start wsnode packet capture, for ${capture_time}s---"
    echo "start wsnode packet capture, for ${capture_time}s..."
    gnome-terminal --window --title="Node Capture" --geometry=90x24+200+0 -- \
      sudo java -jar $silabspti -ip=$wsnode0_netif,$wsnode05_netif,$wsnode2_netif -time=$(($capture_time*1000)) -format=pcapng_wisun -out=${nodes_pti_cap_file}

    # ------start wsnode join_fan10-------
    echo "----------------------------start wsnode0 join_fan10------------------------"
    time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode0 
    time_1=$(date +%s%N); echo "send wsnode0 join_fan10: $((($time_1 - $time_0)/1000000))ms";
    TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
    TIME_NODE0_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
    echo "node0 start wisun join_fan10 at: $TIME_NODE0_JOIN_FAN10"
    display_wait_progress $step0_time;
    echo "-----------------------------------------------------------------------------"
    echo "----------------------------start wsnode05 at ran1 join_fan10------------------------"
    time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode05 
    time_1=$(date +%s%N); echo "send wsnode05 join_fan10: $((($time_1 - $time_0)/1000000))ms";
    TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
    TIME_NODE05_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
    echo "node05 start wisun join_fan10 at: $TIME_NODE05_JOIN_FAN10"
    display_wait_progress $step1_time;
    echo "-----------------------------------------------------------------------------"
    echo "----------------------------start wsnode1 at ran2 join_fan10------------------------"
    time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode1 
    time_1=$(date +%s%N); echo "send wsnode1 join_fan10: $((($time_1 - $time_0)/1000000))ms";
    TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
    TIME_NODE1_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
    echo "node1 start wisun join_fan10 at: $TIME_NODE1_JOIN_FAN10"
    echo "----------------------------start wsnode2 at ran3 join_fan10------------------------"
    time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode2 
    time_1=$(date +%s%N); echo "send wsnode2 join_fan10: $((($time_1 - $time_0)/1000000))ms";
    TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
    TIME_NODE2_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
    echo "node2 start wisun join_fan10 at: $TIME_NODE2_JOIN_FAN10"
    display_wait_progress $step2_time;
    echo "-----------------------------------------------------------------------------"

    echo "-----------------------------------------------------------------------------"
    for index in $(seq 1 3); do
        echo "--------ping round $index--------------------------------"
        time_0=$(date +%s%N); echo "wisun ping $wsnode2_ipaddr 1380" > $wsnode0 
        TIME_PING_DUT=$(($time_0/1000000-$time_start_test))
        TIME_NODE0_PING_DUT=$(echo "$TIME_PING_DUT / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
        echo "ping from node0@rank1 to node2@rank3 at: $TIME_NODE0_PING_DUT"; sleep 5
        time_0=$(date +%s%N); echo "wisun ping $wsnode2_ipaddr 1380" > $wsnode05 
        TIME_PING_DUT=$(($time_0/1000000-$time_start_test))
        TIME_NODE05_PING_DUT=$(echo "$TIME_PING_DUT / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
        echo "ping from node05@rank1 to node2@rank3 at: $TIME_NODE05_PING_DUT"; sleep 5
        time_0=$(date +%s%N); echo "wisun ping $wsnode0_ipaddr 1380" > $wsnode2 
        TIME_PING_DUT=$(($time_0/1000000-$time_start_test))
        TIME_NODE2_PING_DUT=$(echo "$TIME_PING_DUT / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
        echo "ping from node2@rank3 to node0@rank1 at: $TIME_NODE2_PING_DUT"; sleep 5
        time_0=$(date +%s%N); echo "wisun ping $wsnode05_ipaddr 1380" > $wsnode2 
        TIME_PING_DUT=$(($time_0/1000000-$time_start_test))
        TIME_NODE2_PING_DUT=$(echo "$TIME_PING_DUT / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
        echo "ping from node2@rank3 to node05@rank1 at: $TIME_NODE2_PING_DUT"; sleep 5
    done
    echo "-----------------------------------------------------------------------------"
    display_wait_progress $(($step3_time-20*3));

    # check session id of serial port and wsbrd(ssh RPi) and kill them
    wsbrd_id=$(ps -u | grep cd | grep 'sudo wsbrd -F' | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
    echo "kill wsbrd window: $wsbrd_id, actually wsbrd is still running on remote RPi"; kill $wsbrd_id;
    # -------------------------------------------------------------------------------------------------
    # copy border router host/rcp received message pcapng file from RapspberryPi
    # prerequiste is uncomment "pcap_file = /tmp/wisun_dump.pcapng" in wsbrd.conf in RPi
    # -------------------------------------------------------------------------------------------------
    scp ${BRRPI_usr}@${BRRPI_ip}:/tmp/wisun_dump.pcapng ${wsbrd_cap_file}
fi



# check the thread number of wsbrd
# ssh_check_and_kill_wsbrd $BRRPI_usr $BRRPI_ip;

echo "----------------------------------Test Pass/Fail Analysis------------------------------------"
# -------------------------------------------------------------------------------------------------
# ------- post test analysis begin based on ${wsbrd_cap_file} and ${nodes_pti_cap_file}
# -------------------------------------------------------------------------------------------------
# wisun.uttie.type: "Frame Type: PAN Advertisement (0)"           - PA
#                   "Frame Type: PAN Advertisement Solicit (1)"   - PAS
#                   "Frame Type: PAN Configuration (2)"           - PC
#                   "Frame Type: PAN Configuration Solicit (3)"   - PCS
#                   "Frame Type: Data (4)"                        - Data
#                   "Frame Type: Acknowledgment (5)"              - ACK
#                   "Frame Type: EAPOL (6)"                       - EAPOL
# -------------------------------------------------------------------------------------------------
# -------------------------------------------------------------------------------------------------
# eapol.type:       "Type: EAP Packet (0)"           - 
#                   "Type: Key (3)"                               - 
# -------------------------------------------------------------------------------------------------
# output.csv Packet Fields:
# # the 1st 2 options in CSV_PACKET_FIELD_TABLE MUST be "-e frame.number -e frame.time_epoch"
# -------------------------------------------------------------------------------------------------
CSV_PACKET_FIELD_TABLE=(
 1:  frame.number                                    2:  frame.time_epoch                          
 3:  wpan.src64                                      4:  wpan.dst64                                
 5:  frame.protocols                                 6:  wisun.uttie.type                          
 7:  eapol.type                                      8:  wlan_rsna_eapol.keydes.data               
 9:  tls.record.content_type                         10: wlan_rsna_eapol.keydes.key_info           
 11: eapol.keydes.key_len                            12: eap.code                                  
 13: eap.type                                        14: tls.handshake.type                        
 15: eapol.keydes.replay_counter                     16: wlan_rsna_eapol.keydes.nonce              
 17: eapol.keydes.key_iv                             18: wlan_rsna_eapol.keydes.rsc                
 19: wlan_rsna_eapol.keydes.mic                      20: wlan_rsna_eapol.keydes.data_len           
 21: wisun.eapol_relay.sup                           22: wisun.eapol_relay.kmp_id                  
 23: wisun.eaie.eui                                  24: wpan.mpx.kmp.id                                        
 25: icmpv6.type                                     26: icmpv6.code                                        
 27: icmpv6.rpl.dis.flags                            28: icmpv6.reserved                                        
 29: ipv6.src                                        30: ipv6.dst                                         
 31: icmpv6.rpl.dio.instance                         32: icmpv6.rpl.dio.version                                    
 33: icmpv6.rpl.dio.rank                             34: icmpv6.rpl.dio.flag                                        
 35: icmpv6.rpl.dio.dtsn                             36: icmpv6.rpl.dio.dagid                                        
 37: icmpv6.rpl.opt.type                             38: icmpv6.rpl.opt.length                                        
 39: icmpv6.rpl.opt.config.flag                      40: icmpv6.rpl.opt.config.interval_double                                        
 41: icmpv6.rpl.opt.config.interval_min              42: icmpv6.rpl.opt.config.redundancy                                      
 43: icmpv6.rpl.opt.config.max_rank_inc              44: icmpv6.rpl.opt.config.min_hop_rank_inc                                        
 45: icmpv6.rpl.opt.config.ocp                       46: icmpv6.rpl.opt.config.rsv                                        
 47: icmpv6.rpl.opt.config.def_lifetime              48: icmpv6.rpl.opt.config.lifetime_unit                                        
 49: icmpv6.rpl.opt.prefix.length                    50: icmpv6.rpl.opt.prefix.flag                                 
 51: icmpv6.rpl.opt.prefix.valid_lifetime            52: icmpv6.rpl.opt.prefix.preferred_lifetime                                       
 53: icmpv6.rpl.opt.reserved                         54: icmpv6.rpl.opt.prefix                                        
 55: dhcpv6.duid.bytes                               56: icmpv6.nd.ns.target_address                                        
 57: icmpv6.opt.type                                 58: icmpv6.opt.length  
 59: icmpv6.opt.aro.status                           60: icmpv6.opt.reserved    
 61: icmpv6.opt.aro.registration_lifetime            62: icmpv6.opt.aro.eui64                                  
 63: dhcpv6.iaaddr.ip                                64: dhcpv6.xid                                       
 65: dhcpv6.duidll.link_layer_addr                   66: dhcpv6.duid.type                                           
 67: dhcpv6.duidll.hwtype                            68: dhcpv6.iaid                                             
 69: dhcpv6.iaid.t1                                  70: dhcpv6.iaid.t2                                       
 71: dhcpv6.option.type                              72: dhcpv6.elapsed_time                                   
 73: dhcpv6.msgtype                                  74: dhcpv6.linkaddr                                  
 75: dhcpv6.peeraddr                                 76: --                                           
 77: --                                              78: --                                        
 79: --                                              80: --                                 
 81: --                                              82: --                                  
 83: --                                              84: --                             
 85: --                                              86: --                                        
 87: --                                              88: --                                        
 89: --                                              90: --                                 
 91: --                                              92: --                                  
 93: --                                              94: --                             
 95: --                                              96: --                                        
 97: --                                              98: --                                        
 99: --                                              100: --                                 
 ); 
# -------------------------------------------------------------------------------------------------
echo "------------convert Border router packet captures to csv-------------------"
EXTRACT_OPTIONS="";
for idx in $(seq 1 2 ${#CSV_PACKET_FIELD_TABLE[@]}); do
    field=${CSV_PACKET_FIELD_TABLE[$idx]};
    if [ "$field" != "--" ]; then 
        EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e $field";
    fi
done

tshark -r ${wsbrd_cap_file} -T fields $EXTRACT_OPTIONS > $BRcsvFile;
tshark_mod $BRcsvFile  $BRcsvFile

echo "-------------convert Node router packet captures to csv---------------------"
# text2pcap -q -t %H:%M:%S. -l 230 -n Node0Cap_PAN-PA-SELECT-2_0314_18-12.txt Node0Cap_PAN-PA-SELECT-2_0314_18-12.pcapng
# -n : output file format of pcapng not default pcap format
# -l : link-layer type number; default is 1 (Ethernet). see https://www.tcpdump.org/linktypes.html for other
tshark -r ${nodes_pti_cap_file} -T fields $EXTRACT_OPTIONS > $NodeCsvFile
tshark_mod $NodeCsvFile  $NodeCsvFile

synchronize_node_cap_to_Br_cap $BRcsvFile $NodeCsvFile

#
# -------------------------------------------------------------------------------------------------
# Step1 :   Test Bed Devices A-D and E-H are reset to boot-up phase and started.
#           Test Bed Devices A-D and E-H perform PAN Discovery 
# -------------------------------------------------------------------------------------------------
# Step2 :   Test Bed Devices A-D and E-H perform Security Bootstrapping
#           Test Bed Devices A-D and E-H perform ROLL/RPL Route Establishment through DIO 
#               processing and candidate parent selection (or Layer 2 Routing Establishment)
#           Test Bed Devices A-D and E-H perform DHCPv6 
#           Test Bed Devices A-D and E-H perform 6LoWPAN ND and are in State 5
# -------------------------------------------------------------------------------------------------
# Step3 :   Test Bed Device E sends ICMPv6 Echo Request to the Border Router DUT with no payload
#           Border Router DUT sends ICMPv6 Echo Reply to Test Bed Device E
# -------------------------------------------------------------------------------------------------
# initialize the pass/fail array, steps_pass[0] related to the whole test PASS/FAIL
steps_pass=(); steps_pass[0]=1;

# Test Bed Devices A-D and E-H perform PAN Discovery
##echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "DUT(Border Router) received PAS from TBD A..." 
"time range:"       "0.00000"                   "300.00000"
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]})
#echo "DUT(Border Router) received PAS from TBU @: ${packet_checked[1]}"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "after PAS sent, PA observed (on TBD A) within DISC_IMIN = ${DISC_IMIN}s" 
"time range:"       $time_checked               $(echo "$time_checked + $DISC_IMIN.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          0
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

# Test Bed Devices A-D and E-H perform Security Bootstrapping
#echo "-------------------------------------------------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                3
"match items:"      "wlan_rsna_eapol.keydes.data"  "xxxx"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=(${packet_checked[@]})
DUT_receive_EAPOLEAP_EAPOL_KEY=${packet_checked[7]};
echo "DUT_receive_EAPOLEAP_EAPOL-KEY: $DUT_receive_EAPOLEAP_EAPOL_KEY";

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP Request Identity to TBD A" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response Identity" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "DUT border router sends EAP Response Identity" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response EAP-TLS Client Hello to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
"match items:"      "tls.handshake.type"        1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-TLS: Server Hello" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls:x509sat:x509sat:pkcs-1:x509ce:x509ce:x509ce:cms:x509ce:x509sat:x509sat:x509sat"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
"match items:"      "tls.handshake.type"        "2,11,12,13,14"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response EAP-TLS to BR DUT, Certificate, Client Key Exchange" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls:x509sat:x509sat:pkcs-1:x509ce:x509ce:x509ce:cms:x509ce"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-Request EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
"match items:"      "tls.record.content_type"   "20,22"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP-Response after DUT send EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-Success" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  3
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
time_finish0_DISCOVER_SEQURITY=$time_checked;


# Test Bed Devices A-D perform DHCPv6 
echo "";echo "---- Test Bed Devices A-D perform DHCPv6 ----------------"
#echo "-----------------"
time_checked=$time_finish0_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT receives DHCPv6 Client Solicit from TBU A" 
"time range:"       $time_checked           $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"            $wsnode0_mac
"match items:"      "wpan.dst64"            $BRRPI_mac
"match items:"      "frame.protocols"       "wpan:6lowpan:ipv6:udp:dhcpv6"
"match items:"      "dhcpv6.xid"                xxxx
"match items:"      "dhcpv6.duid.type"          3
"match items:"      "dhcpv6.duidll.hwtype"      6
"match items:"      "dhcpv6.iaid"               xxxx
"match items:"      "dhcpv6.iaid.t1"            xxxx 
"match items:"      "dhcpv6.iaid.t2"            xxxx
"match items:"      "dhcpv6.option.type"        xxxx
"match items:"      "dhcpv6.elapsed_time"       xxxx
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=($packet_checked);
transaction_id=${packet_checked[63]}; echo "transaction id: $transaction_id"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends DHCPv6 Reply with assigned IPv6 address" 
"time range:"       $time_checked           $(echo "$time_checked + 100.000000" | bc -l)
"match items:"      "wpan.src64"            $BRRPI_mac
"match items:"      "wpan.dst64"            $wsnode0_mac
"match items:"      "frame.protocols"       "wpan:6lowpan:ipv6:udp:dhcpv6"
"match items:"      "dhcpv6.xid"                $transaction_id
"match items:"      "dhcpv6.duid.type"          "3,3"
"match items:"      "dhcpv6.duidll.hwtype"      "27,6"
"match items:"      "dhcpv6.iaid"               xxxx
"match items:"      "dhcpv6.iaid.t1"            xxxx 
"match items:"      "dhcpv6.iaid.t2"            xxxx
"match items:"      "dhcpv6.option.type"        xxxx
"match items:"      "dhcpv6.iaaddr.ip"          xxxx
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=($packet_checked); wsnode0_ipaddr=${packet_checked[62]};
echo "wsnode0 was assigned IPv6 address: $wsnode0_ipaddr"



# TBDes A-D perform ROLL/RPL Route Establishment through DIO processing and candidate parent selection 
echo "";echo "---- TBD A perform ROLL/RPL Route Establishment through DIO processing ----------------"
#echo "-----------------"
time_checked=$time_finish0_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${BRcsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "OPTIONAL-PASS: BR DUT receives “RPL DIS” from joiner TBDes A-D with the parameters" 
"time range:"       $time_checked      $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"        $wsnode0_mac
"match items:"      "wpan.dst64"        --
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       0
"match items:"      "icmpv6.rpl.dis.flags"      0
"match items:"      "icmpv6.reserved"           00
"match items:"      "ipv6.src"          fe80::----
"match items:"      "ipv6.dst"          ff02::----
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]});
#echo "packet_checked: $packet_checked"

#echo "-----------------"
time_checked=$time_finish0_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT sends initial RPL DIO to joiner TBDes A-D with the parameters" 
"time range:"       $time_checked      $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"        $BRRPI_mac
"match items:"      "wpan.dst64"        --
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       1
"match items:"      "ipv6.src"          fe80::----
"match items:"      "ipv6.dst"          ff02::----
"match items:"      "icmpv6.rpl.dio.instance"       xxxx                      
"match items:"      "icmpv6.rpl.dio.version"        xxxx                                
"match items:"      "icmpv6.rpl.dio.rank"           196
"match items:"      "icmpv6.rpl.dio.flag"           "0x88,0x00"
"match items:"      "icmpv6.rpl.dio.dtsn"           xxxx
"match items:"      "icmpv6.reserved"               "00"
"match items:"      "icmpv6.rpl.dio.dagid"          fd00:6868:6868:----
"match items:"      "icmpv6.rpl.opt.type"           "4,8"
"match items:"      "icmpv6.rpl.opt.length"         "14,30" 
"match items:"      "icmpv6.rpl.opt.config.flag"    "0x07"      
"match items:"      "icmpv6.rpl.opt.config.interval_double"     2      
"match items:"      "icmpv6.rpl.opt.config.interval_min"        15      
"match items:"      "icmpv6.rpl.opt.config.redundancy"          0      
#"match items:"      "icmpv6.rpl.opt.config.max_rank_inc"        2048
#"match items:"      "icmpv6.rpl.opt.config.min_hop_rank_inc"    196  
"match items:"      "icmpv6.rpl.opt.config.ocp"                 1   
"match items:"      "icmpv6.rpl.opt.config.rsv"                 0   
"match items:"      "icmpv6.rpl.opt.config.def_lifetime"        120 
"match items:"      "icmpv6.rpl.opt.config.lifetime_unit"       60  
"match items:"      "icmpv6.rpl.opt.prefix.length"              64    
"match items:"      "icmpv6.rpl.opt.prefix.flag"                0x20    
"match items:"      "icmpv6.rpl.opt.prefix.valid_lifetime"      0
"match items:"      "icmpv6.rpl.opt.prefix.preferred_lifetime"  0    
#"match items:"      "icmpv6.rpl.opt.reserved"                   1 
"match items:"      "icmpv6.rpl.opt.prefix"                     fd00:6868:6868----  
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]});
#echo "packet_checked: $packet_checked"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "TBD A send a RPL DAO to the BR DUT which contents are as described in ROLL-RPL-JOIN-1" 
"time range:"       $time_checked      $(echo "$time_checked + 250.000000" | bc -l)
"match items:"      "wpan.src64"        $wsnode0_mac
"match items:"      "wpan.dst64"        $BRRPI_mac
"match items:"      "frame.protocols"   "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       2
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT reply a RPL Acknowlegement back to TBU A" 
"time range:"       $time_checked      $(echo "$time_checked + 100.000000" | bc -l)
"match items:"      "wpan.src64"        $BRRPI_mac
"match items:"      "wpan.dst64"        $wsnode0_mac
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       3
"match items:"      "ipv6.src"          $BRRPI_ipv6
"match items:"      "ipv6.dst"          fd00:6868:6868:0:8e1f:645e:40bb:1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE


#---------- Test Bed Devices A-D perform 6LoWPAN ND and are in State 5
echo "";echo "---- TBD A perform 6LoWPAN ND and are in State 5 ----------------"
#echo "-----------------"
time_checked=$time_finish0_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Test bed device A sends ND Solicit to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 250.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               135
"match items:"      "icmpv6.code"               0
"match items:"      "icmpv6.nd.ns.target_address"           xxxx
"match items:"      "icmpv6.opt.type"                       --
"match items:"      "icmpv6.opt.length"                     -- 
"match items:"      "icmpv6.opt.aro.status"                 --      
"match items:"      "icmpv6.opt.reserved"                   --
"match items:"      "icmpv6.opt.aro.registration_lifetime"  --      
"match items:"      "icmpv6.opt.aro.eui64"                  --
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=(${packet_checked[@]})
BRRPI_lladdr=${packet_checked[29]}; wsnode0_lladdr=${packet_checked[28]};
echo "BRRPI_lladdr: $BRRPI_lladdr"
echo "wsnode0_lladdr: $wsnode0_lladdr"


#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Test bed device A sends “NS with ARO” to Border Router DUT (sent with AR=1)" 
"time range:"       $time_checked               $(echo "$time_checked + 50.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               135
"match items:"      "icmpv6.code"               0
"match items:"      "icmpv6.nd.ns.target_address"           $BRRPI_lladdr
"match items:"      "icmpv6.opt.type"                       33
"match items:"      "icmpv6.opt.length"                     2 
"match items:"      "icmpv6.opt.aro.status"                 0      
"match items:"      "icmpv6.opt.reserved"                   1
"match items:"      "icmpv6.opt.aro.registration_lifetime"  xxxx      
"match items:"      "icmpv6.opt.aro.eui64"                  $wsnode0_mac
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner test bed device sends DAO to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               155
"match items:"      "icmpv6.code"               2
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner test bed device sends DAO to Border Router DUT and receives DAO-ACK." 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               155
"match items:"      "icmpv6.code"               3
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE






#---------- Test Bed Devices E-H perform Discover and Security Bootstrapping
echo "";echo "---- Test Bed Devices E-H perform Discover and Security Bootstrapping ----------------"
#---------- TBDes E-H perform ROLL/RPL Route Establishment through DIO processing and candidate parent selection
echo "---- TBDes E-H perform ROLL/RPL Route Establishment ----------------"

##echo "-----------------"
time_checked=$step0_time
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "DUT(Border Router) received PAS from TBD A..." 
"time range:"       "0.00000"                   "300.00000"
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]})
#echo "DUT(Border Router) received PAS from TBU @: ${packet_checked[1]}"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "after PAS sent, PA observed (on TBD A) within DISC_IMIN = ${DISC_IMIN}s" 
"time range:"       $time_checked               $(echo "$time_checked + $DISC_IMIN.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          0
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

# Test Bed Devices A-D and E-H perform Security Bootstrapping
#echo "-------------------------------------------------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                3
"match items:"      "wlan_rsna_eapol.keydes.data"  "xxxx"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=(${packet_checked[@]})
DUT_receive_EAPOLEAP_EAPOL_KEY=${packet_checked[7]};
echo "DUT_receive_EAPOLEAP_EAPOL-KEY: $DUT_receive_EAPOLEAP_EAPOL_KEY";

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP Request Identity to TBD A" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response Identity" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "DUT border router sends EAP Response Identity" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response EAP-TLS Client Hello to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
"match items:"      "tls.handshake.type"        1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-TLS: Server Hello" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls:x509sat:x509sat:pkcs-1:x509ce:x509ce:x509ce:cms:x509ce:x509sat:x509sat:x509sat"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
"match items:"      "tls.handshake.type"        "2,11,12,13,14"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP Response EAP-TLS to BR DUT, Certificate, Client Key Exchange" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls:x509sat:x509sat:pkcs-1:x509ce:x509ce:x509ce:cms:x509ce"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-Request EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
"match items:"      "tls.record.content_type"   "20,22"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner (TBD A) sends EAP-Response after DUT send EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends EAP-Success" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  3
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
time_finish05_DISCOVER_SEQURITY=$time_checked;


# Test Bed Devices A-D perform DHCPv6 
echo "";echo "---- Test Bed Devices A-D perform DHCPv6 ----------------"
#echo "-----------------"
time_checked=$time_finish05_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT receives DHCPv6 Client Solicit from TBU A" 
"time range:"       $time_checked           $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"            $wsnode05_mac
"match items:"      "wpan.dst64"            $BRRPI_mac
"match items:"      "frame.protocols"       "wpan:6lowpan:ipv6:udp:dhcpv6"
"match items:"      "dhcpv6.xid"                xxxx
"match items:"      "dhcpv6.duid.type"          3
"match items:"      "dhcpv6.duidll.hwtype"      6
"match items:"      "dhcpv6.iaid"               xxxx
"match items:"      "dhcpv6.iaid.t1"            xxxx 
"match items:"      "dhcpv6.iaid.t2"            xxxx
"match items:"      "dhcpv6.option.type"        xxxx
"match items:"      "dhcpv6.elapsed_time"       xxxx
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=($packet_checked);
transaction_id=${packet_checked[63]}; echo "transaction id: $transaction_id"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Border Router DUT sends DHCPv6 Reply with assigned IPv6 address" 
"time range:"       $time_checked           $(echo "$time_checked + 100.000000" | bc -l)
"match items:"      "wpan.src64"            $BRRPI_mac
"match items:"      "wpan.dst64"            $wsnode05_mac
"match items:"      "frame.protocols"       "wpan:6lowpan:ipv6:udp:dhcpv6"
"match items:"      "dhcpv6.xid"                $transaction_id
"match items:"      "dhcpv6.duid.type"          "3,3"
"match items:"      "dhcpv6.duidll.hwtype"      "27,6"
"match items:"      "dhcpv6.iaid"               xxxx
"match items:"      "dhcpv6.iaid.t1"            xxxx 
"match items:"      "dhcpv6.iaid.t2"            xxxx
"match items:"      "dhcpv6.option.type"        xxxx
"match items:"      "dhcpv6.iaaddr.ip"          xxxx
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=($packet_checked); wsnode05_ipaddr=${packet_checked[62]};
echo "wsnode05 was assigned IPv6 address: $wsnode05_ipaddr"



# TBDes A-D perform ROLL/RPL Route Establishment through DIO processing and candidate parent selection 
echo "";echo "---- TBD A perform ROLL/RPL Route Establishment through DIO processing ----------------"
#echo "-----------------"
time_checked=$time_finish05_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${BRcsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "OPTIONAL-PASS: BR DUT receives “RPL DIS” from joiner TBDes A-D with the parameters" 
"time range:"       $time_checked      $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"        $wsnode05_mac
"match items:"      "wpan.dst64"        --
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       0
"match items:"      "icmpv6.rpl.dis.flags"      0
"match items:"      "icmpv6.reserved"           00
"match items:"      "ipv6.src"          fe80::----
"match items:"      "ipv6.dst"          ff02::----
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]});
#echo "packet_checked: $packet_checked"

#echo "-----------------"
time_checked=$time_finish05_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT sends initial RPL DIO to joiner TBDes A-D with the parameters" 
"time range:"       $time_checked      $(echo "$time_checked + 300.000000" | bc -l)
"match items:"      "wpan.src64"        $BRRPI_mac
"match items:"      "wpan.dst64"        --
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       1
"match items:"      "ipv6.src"          fe80::----
"match items:"      "ipv6.dst"          ff02::----
"match items:"      "icmpv6.rpl.dio.instance"       xxxx                      
"match items:"      "icmpv6.rpl.dio.version"        xxxx                                
"match items:"      "icmpv6.rpl.dio.rank"           196
"match items:"      "icmpv6.rpl.dio.flag"           "0x88,0x00"
"match items:"      "icmpv6.rpl.dio.dtsn"           xxxx
"match items:"      "icmpv6.reserved"               "00"
"match items:"      "icmpv6.rpl.dio.dagid"          fd00:6868:6868:----
"match items:"      "icmpv6.rpl.opt.type"           "4,8"
"match items:"      "icmpv6.rpl.opt.length"         "14,30" 
"match items:"      "icmpv6.rpl.opt.config.flag"    "0x07"      
"match items:"      "icmpv6.rpl.opt.config.interval_double"     2      
"match items:"      "icmpv6.rpl.opt.config.interval_min"        15      
"match items:"      "icmpv6.rpl.opt.config.redundancy"          0      
#"match items:"      "icmpv6.rpl.opt.config.max_rank_inc"        2048
#"match items:"      "icmpv6.rpl.opt.config.min_hop_rank_inc"    196  
"match items:"      "icmpv6.rpl.opt.config.ocp"                 1   
"match items:"      "icmpv6.rpl.opt.config.rsv"                 0   
"match items:"      "icmpv6.rpl.opt.config.def_lifetime"        120 
"match items:"      "icmpv6.rpl.opt.config.lifetime_unit"       60  
"match items:"      "icmpv6.rpl.opt.prefix.length"              64    
"match items:"      "icmpv6.rpl.opt.prefix.flag"                0x20    
"match items:"      "icmpv6.rpl.opt.prefix.valid_lifetime"      0
"match items:"      "icmpv6.rpl.opt.prefix.preferred_lifetime"  0    
#"match items:"      "icmpv6.rpl.opt.reserved"                   1 
"match items:"      "icmpv6.rpl.opt.prefix"                     fd00:6868:6868----  
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]});
#echo "packet_checked: $packet_checked"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "TBD A send a RPL DAO to the BR DUT which contents are as described in ROLL-RPL-JOIN-1" 
"time range:"       $time_checked      $(echo "$time_checked + 250.000000" | bc -l)
"match items:"      "wpan.src64"        $wsnode05_mac
"match items:"      "wpan.dst64"        $BRRPI_mac
"match items:"      "frame.protocols"   "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       2
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "BR DUT reply a RPL Acknowlegement back to TBU A" 
"time range:"       $time_checked      $(echo "$time_checked + 100.000000" | bc -l)
"match items:"      "wpan.src64"        $BRRPI_mac
"match items:"      "wpan.dst64"        $wsnode05_mac
"match items:"      "frame.protocols"   "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"  4
"match items:"      "icmpv6.type"       155
"match items:"      "icmpv6.code"       3
"match items:"      "ipv6.src"          $BRRPI_ipv6
"match items:"      "ipv6.dst"          fd00:6868:6868:0:8e1f:645e:40bb:4
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE


#---------- Test Bed Devices A-D perform 6LoWPAN ND and are in State 5
echo "";echo "---- TBD A perform 6LoWPAN ND and are in State 5 ----------------"
#echo "-----------------"
time_checked=$time_finish05_DISCOVER_SEQURITY;
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Test bed device E sends ND Solicit to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 250.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               135
"match items:"      "icmpv6.code"               0
"match items:"      "icmpv6.nd.ns.target_address"           $BRRPI_lladdr
"match items:"      "icmpv6.opt.type"                       --
"match items:"      "icmpv6.opt.length"                     -- 
"match items:"      "icmpv6.opt.aro.status"                 --      
"match items:"      "icmpv6.opt.reserved"                   --
"match items:"      "icmpv6.opt.aro.registration_lifetime"  --      
"match items:"      "icmpv6.opt.aro.eui64"                  --
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=(${packet_checked[@]})
wsnode05_lladdr=${packet_checked[28]};
echo "wsnode05_lladdr: $wsnode0_lladdr"

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Test bed device A sends “NS with ARO” to Border Router DUT (sent with AR=1)" 
"time range:"       $time_checked               $(echo "$time_checked + 50.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               135
"match items:"      "icmpv6.code"               0
"match items:"      "icmpv6.nd.ns.target_address"           $BRRPI_lladdr
"match items:"      "icmpv6.opt.type"                       33
"match items:"      "icmpv6.opt.length"                     2 
"match items:"      "icmpv6.opt.aro.status"                 0      
"match items:"      "icmpv6.opt.reserved"                   1
"match items:"      "icmpv6.opt.aro.registration_lifetime"  xxxx      
"match items:"      "icmpv6.opt.aro.eui64"                  $wsnode05_mac
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner test bed device sends DAO to Border Router DUT" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode05_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               155
"match items:"      "icmpv6.code"               2
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

#echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step${#steps_pass[@]}"
"Step Description:" "Joiner test bed device sends DAO to Border Router DUT and receives DAO-ACK." 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode05_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:icmpv6"
"match items:"      "wisun.uttie.type"          4
"match items:"      "icmpv6.type"               155
"match items:"      "icmpv6.code"               3
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE






#---------- TBD A/E send ICPMv6 Echo Request to the LBR DUT 
echo "---- TBD A/E send ICPMv6 Echo Request to the LBR DUT ----------------"
time_checked=$(($step0_time+$step1_time+$step2_time+20-1));
for index in $(seq 1 3); do
    echo "-------ping round $index----------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "Test bed device A is able to send ICMPv6 Echo Request to the LBR DUT using DFE" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode0_lladdr,$wsnode0_ipaddr
    "match items:"      "ipv6.dst"                  $BRRPI_lladdr,$wsnode2_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               128
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "6LoWPAN frame from DUT containing the ICMPv6 Echo Reply" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode2_lladdr,$wsnode2_ipaddr
    "match items:"      "ipv6.dst"                  $wsnode1_lladdr,$wsnode0_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               129
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "Test bed device E is able to send ICMPv6 Echo Request to the LBR DUT using DFE" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode05_lladdr,$wsnode05_ipaddr
    "match items:"      "ipv6.dst"                  $BRRPI_lladdr,$wsnode2_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               128
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "6LoWPAN frame from DUT containing the ICMPv6 Echo Reply" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode2_lladdr,$wsnode2_ipaddr
    "match items:"      "ipv6.dst"                  $wsnode1_lladdr,$wsnode05_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               129
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE


    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "Test bed device A is able to send ICMPv6 Echo Request to the LBR DUT using DFE" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode2_lladdr,$wsnode2_ipaddr
    "match items:"      "ipv6.dst"                  $wsnode1_lladdr,$wsnode0_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               128
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "6LoWPAN frame from DUT containing the ICMPv6 Echo Reply" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode0_lladdr,$wsnode0_ipaddr
    "match items:"      "ipv6.dst"                  $BRRPI_lladdr,$wsnode2_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               129
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "Test bed device E is able to send ICMPv6 Echo Request to the LBR DUT using DFE" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode2_lladdr,$wsnode2_ipaddr
    "match items:"      "ipv6.dst"                  $wsnode1_lladdr,$wsnode05_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               128
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
    #echo "-----------------"
    STEP_PASSFAIL_Criteria=(
    "output csv file:"  ${NodeCsvFile}                                                          
    "Step number:"      "Step${#steps_pass[@]}"
    "Step Description:" "6LoWPAN frame from DUT containing the ICMPv6 Echo Reply" 
    "time range:"       $time_checked               $(echo "$time_checked + 7.000000" | bc -l)
    "match items:"      "ipv6.src"                  $wsnode05_lladdr,$wsnode05_ipaddr
    "match items:"      "ipv6.dst"                  $BRRPI_lladdr,$wsnode2_ipaddr
    "match items:"      "frame.protocols"           "wpan:6lowpan:data:data:ipv6:ipv6.hopopts:ipv6:ipv6.fraghdr:icmpv6:data"
    "match items:"      "icmpv6.type"               129
    "match items:"      "icmpv6.code"               0
    );
    step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

done







# -------------------------------------------------------------------------------------
echo "------------------------------- all test done -------------------------------------------"
echo "----Total: ${#steps_pass[@]} test steps done"
test_passfail=0;
for passfail in ${steps_pass[@]}; do
    if [ "$passfail" -eq "0" ]; then
        test_passfail=$(($test_passfail+1));
    fi 
done

if [ "$test_passfail" -eq "0" ]; then 
    echo "----TEST [$TEST_CASE_NAME] : all test items PASS";
else
    echo "----TEST [$TEST_CASE_NAME] : FAIL, ${test_passfail}/${#steps_pass[@]} test items FAIL";
fi
echo "----TEST [$TEST_CASE_NAME] : complete .............................................................."
echo ""
echo ""

