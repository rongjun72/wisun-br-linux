source br_cert_funcions.sh
# -----------------------------------------------------------------------
# Test case :   Border Router as DUT [PAN-KEY-TLS-8]
# -----------------------------------------------------------------------
# Description:  
# This test case verifies the ability of Border Router DUT node to 
# exchange EAPOL frames with Joiner node through a forwarding Router.
# -----------------------------------------------------------------------
# Test Bed Configuration:
# Border Router (DUT) is configured to work with a RADIUS authentication 
# server. Test Bed Device A Router has joined the PAN.
# The test bed is configured as follows:
#   d. Border router DUT configured for PAN A
#   e. Device A joined to PAN A at Rank 1
#   f. Device E introduced at Rank 2 in join state 1
# -----------------------------------------------------------------------
TEST_CASE_NAME="PAN-KEY-TLS-8"



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
TEST_TIME="0401_11-47";
TEST_TIME=$(date "+%m%d_%H-%M");

time_start_test=$(($(date +%s%N)/1000000));
nodes_pti_cap_file=${LOG_PATH}/NodesCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
wsbrd_cap_file=${LOG_PATH}/BrCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
NodeCsvFile=${LOG_PATH}/output_node.csv;
time_checked=0;
packet_checked="";

echo "----TEST [$TEST_CASE_NAME] start ..................................................................."
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
wisun_node_set $wsnode0 $wisun_domain $wisun_mode $wisun_class
wisun_node_set $wsnode05 $wisun_domain $wisun_mode $wisun_class
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

echo "----------------------------start wsbrd application on Raspberry Pi...-------"
TIME_START_WSBRD=$(($(date +%s%N)/1000000 - $time_start_test)); # uint in ms
TIME_START_WSBRD=$(echo "$TIME_START_WSBRD / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
echo "start wsbrd at time: $TIME_START_WSBRD"
ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class


capture_time=400
echo "----------------------------start wsnode packet capture, for ${capture_time}s---"
echo "start wsnode packet capture, for ${capture_time}s..."
gnome-terminal --window --title="Node Capture" --geometry=90x24+200+0 -- \
  sudo java -jar $silabspti -ip=$wsnode0_netif,$wsnode1_netif -time=$(($capture_time*1000)) -format=pcapng_wisun -out=${nodes_pti_cap_file}

# ------start wsnode join_fan10-------
echo "----------------------------start wsnode0 join_fan10------------------------"
time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode0 
time_1=$(date +%s%N); echo "send wsnode0 join_fan10: $((($time_1 - $time_0)/1000000))ms";
TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
TIME_NODE0_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
#echo "node0 start wisun join_fan10 at: $(($TIME_JOIN_FAN10/1000)).$(echo $(($TIME_JOIN_FAN10%1000+1000)) | sed 's/^1//')"
echo "node0 start wisun join_fan10 at: $TIME_NODE0_JOIN_FAN10"
display_wait_progress $(($capture_time/2));
echo "-----------------------------------------------------------------------------"
echo "----------------------------start wsnode1 join_fan10------------------------"
time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode1 
time_1=$(date +%s%N); echo "send wsnode1 join_fan10: $((($time_1 - $time_0)/1000000))ms";
TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test));
TIME_NODE1_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
echo "node1 start wisun join_fan10 at: $TIME_NODE1_JOIN_FAN10"
display_wait_progress $(($capture_time/2));




# check session id of serial port and wsbrd(ssh RPi) and kill them
wsbrd_id=$(ps -u | grep cd | grep 'sudo wsbrd -F' | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
echo "kill wsbrd window: $wsbrd_id, actually wsbrd is still running on remote RPi"; kill $wsbrd_id;
# -------------------------------------------------------------------------------------------------
# copy border router host/rcp received message pcapng file from RapspberryPi
# prerequiste is uncomment "pcap_file = /tmp/wisun_dump.pcapng" in wsbrd.conf in RPi
# -------------------------------------------------------------------------------------------------
scp ${BRRPI_usr}@${BRRPI_ip}:/tmp/wisun_dump.pcapng ${wsbrd_cap_file}




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
 23: wisun.eaie.eui                                  24: --                                        
 25: --                                              26: --                                        
 27: --                                              28: --                                        
 29: --                                              30: --                                        ); 
 
# -------------------------------------------------------------------------------------------------
echo "------------convert Border router packet captures to csv-------------------"
EXTRACT_OPTIONS="";
for idx in $(seq 1 2 ${#CSV_PACKET_FIELD_TABLE[@]}); do
    field=${CSV_PACKET_FIELD_TABLE[$idx]};
    if [ "$field" != "--" ]; then 
        EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e $field";
    fi
done

tshark -r ${wsbrd_cap_file} -T fields $EXTRACT_OPTIONS > ${LOG_PATH}/output_br.csv;
tshark_mod ${LOG_PATH}/output_br.csv  ${LOG_PATH}/output_br.csv


echo "-------------convert Node router packet captures to csv---------------------"
# text2pcap -q -t %H:%M:%S. -l 230 -n Node0Cap_PAN-PA-SELECT-2_0314_18-12.txt Node0Cap_PAN-PA-SELECT-2_0314_18-12.pcapng
# -n : output file format of pcapng not default pcap format
# -l : link-layer type number; default is 1 (Ethernet). see https://www.tcpdump.org/linktypes.html for other
tshark -r ${nodes_pti_cap_file} -T fields $EXTRACT_OPTIONS > ${LOG_PATH}/output_node.csv
tshark_mod ${LOG_PATH}/output_node.csv  ${LOG_PATH}/output_node.csv

synchronize_node_cap_to_Br_cap ${LOG_PATH}/output_br.csv ${LOG_PATH}/output_node.csv

# [PAN-KEY-TLS-8] Pass/Fail Criteria
# -------------------------------------------------------------------------------------------------
# Step1 PASS:   Wireshark capture shows transmission of PA from DUT before DISC_IMIN time passes after receiving the PAS.
#       FAIL:   PAN Advertisement frame fails to be transmitted from DUT within DISC_IMIN seconds of the PAS being received.
# -------------------------------------------------------------------------------------------------
# Step2 PASS:   DUT Border Router PAN Advertisement frame observed
#       FAIL:   No DUT Border Router PAN Advertisement frames observed
# -------------------------------------------------------------------------------------------------
# Step3 PASS:   Steps 1 to 12 of the test procedures are successfully performed
#       FAIL:   Any of Steps 1 to 12 test procedures fail to complete.
# -------------------------------------------------------------------------------------------------
# Step4 PASS:   Wireshark capture shows EAP-TLS exchange.
#       FAIL:   Server Hello is not issued from Border Router DUT
# -------------------------------------------------------------------------------------------------
# initialize the pass/fail array, steps_pass[0] related to the whole test PASS/FAIL
steps_pass=(); steps_pass[0]=1;
# Step 1-2 ----
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step1"
"Step Description:" "DUT(Border Router) received PAS from TBU..." 
"time range:"       "0.00000"                   "60.00000"
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
#packet_checked=(${packet_checked[@]})
#echo "DUT(Border Router) received PAS from TBU @: ${packet_checked[1]}"

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step2"
"Step Description:" "PAS send, and PA observed within DISC_IMIN = ${DISC_IMIN}s" 
"time range:"       $time_checked               $(echo "$time_checked + $DISC_IMIN.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                "--"
"match items:"      "frame.protocols"           "wpan"
"match items:"      "wisun.uttie.type"          0
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE




# Step 3-5 ----
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step3"
"Step Description:" "Joiner issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY" 
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

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step4"
"Step Description:" "Border Router DUT sends EAP Request Identity" 
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

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step5"
"Step Description:" "Joiner sends EAP Response Identity" 
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


# Step 6-7 ----
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step6"
"Step Description:" "Joiner sends EAP Response Identity" 
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

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step7"
"Step Description:" "Joiner sends EAP Response EAP-TLS Client Hello to Border Router DUT" 
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


# Step 8-9 ----
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step8"
"Step Description:" "Border Router DUT sends EAP-TLS: Server Hello" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
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

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step9"
"Step Description:" "Joiner sends EAP Response EAP-TLS to BR DUT, Certificate, Client Key Exchange" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol:tls:x509sat:x509sat:pkcs-1:x509ce:x509ce:x509ce:cms:x509ce"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE


# Step 10-12 ----
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step10"
"Step Description:" "Border Router DUT sends EAP-Request EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
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

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step11"
"Step Description:" "Joiner sends EAP-Response after DUT send EAP-TLS: Change Cipher Spec, Finished" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step12"
"Step Description:" "Border Router DUT sends EAP-Success" 
"time range:"       $time_checked               $(echo "$time_checked + 30.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  3
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE





#---------- EAP-TLS Multi-hop  PAN-KEY-TLS-8 ananlysis really start here
echo "";echo "---- EAP-TLS Multi-hop  PAN-KEY-TLS-8 ananlysis begin ----------------"
# Step 13-16 ---- ] EAP-TLS Multi-hop  analysis ---------------------------------------
time_TBD1_send_PAS=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode1_mac 5 "wpan" 6 "1"));
echo "Node1 send PAS @: ${time_TBD1_send_PAS[@]}"

# ---------------------
time_checked=${time_TBD1_send_PAS[0]}
echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step13"
"Step Description:" "Test Bed Device E Joiner issues a EAPOL-EAP frame" 
"time range:"       $time_checked               $(echo "$time_checked + 60.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode1_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                3
"match items:"      "wlan_rsna_eapol.keydes.data"  "xxxx"
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE
packet_checked=(${packet_checked[@]})
TBD1_send_EAPOL_EAP_KEY=${packet_checked[7]};

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step14"
"Step Description:" "TBD E Joiner issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:udp:wisun.eapol_relay:eapol"
"match items:"      "wisun.uttie.type"          4
"match items:"      "eapol.type"                3
"match items:"      "wlan_rsna_eapol.keydes.data"  $TBD1_send_EAPOL_EAP_KEY
"match items:"      "wisun.eapol_relay.sup"     $wsnode1_mac
"match items:"      "wisun.eapol_relay.kmp_id"  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step15"
"Step Description:" "DUT sends EAP Request Identity to Test Bed Device A via EAPOL-RELAY" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:udp:wisun.eapol_relay:eapol"
"match items:"      "wisun.uttie.type"          4
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  1
"match items:"      "wisun.eapol_relay.sup"     $wsnode1_mac
"match items:"      "wisun.eapol_relay.kmp_id"  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step16"
"Step Description:" "TBD A Router sends to Joiner device TBD E via EAPOL Frame" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $wsnode1_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  1
"match items:"      "wisun.eaie.eui"            $BRRPI_mac
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step17"
"Step Description:" "TBD E sends EAPOL frame to TBD A Router with EAP-Response ID" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode1_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:eapol"
"match items:"      "wisun.uttie.type"          6
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step18"
"Step Description:" "TBD A Router sends EAPOL-RELAY to BR DUT with SUP EUI-64 of Joiner node" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $wsnode0_mac
"match items:"      "wpan.dst64"                $BRRPI_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:udp:wisun.eapol_relay:eapol"
"match items:"      "wisun.uttie.type"          4
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  2
"match items:"      "eap.type"                  1
"match items:"      "wisun.eapol_relay.sup"     $wsnode1_mac
"match items:"      "wisun.eapol_relay.kmp_id"  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step19"
"Step Description:" "BR DUT sends via EAPOL-RELAY EAP Request TLS Start to TBD A Router" 
"time range:"       $time_checked               $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"                $BRRPI_mac
"match items:"      "wpan.dst64"                $wsnode0_mac
"match items:"      "frame.protocols"           "wpan:6lowpan:ipv6:udp:wisun.eapol_relay:eapol"
"match items:"      "wisun.uttie.type"          4
"match items:"      "eapol.type"                0
"match items:"      "eap.code"                  1
"match items:"      "eap.type"                  13
"match items:"      "wisun.eapol_relay.sup"     $wsnode1_mac
"match items:"      "wisun.eapol_relay.kmp_id"  1
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE

echo "-----------------"
STEP_PASSFAIL_Criteria=(
"output csv file:"  ${NodeCsvFile}                                                          
"Step number:"      "Step20"
"Step Description:" "Test Bed Device A Router sends EAPOL Frame to Test Bed Device E Joine" 
"time range:"       $time_checked      $(echo "$time_checked + 10.000000" | bc -l)
"match items:"      "wpan.src64"        $wsnode0_mac
"match items:"      "wpan.dst64"        $wsnode1_mac
"match items:"      "frame.protocols"   "wpan:eapol"
"match items:"      "wisun.uttie.type"  6
"match items:"      "eapol.type"        0
"match items:"      "eap.code"          1
"match items:"      "eap.type"          13
);
step_pass_fail_check STEP_PASSFAIL_Criteria CSV_PACKET_FIELD_TABLE







echo "Total: ${#steps_pass[@]} test steps done"
test_passfail=1;
for passfail in ${steps_pass[@]}; do
    if [ "$passfail" -eq "0" ]; then
        test_passfail=0;
        break;
    fi 
done
if [ "$test_passfail" -ne "0" ]; then 
    echo "----TEST [$TEST_CASE_NAME] : all test items PASS";
else
    echo "----TEST [$TEST_CASE_NAME] : FAIL, some test items FAIL";
fi
echo "----TEST [$TEST_CASE_NAME] complete .............................................................."
echo ""
echo ""

