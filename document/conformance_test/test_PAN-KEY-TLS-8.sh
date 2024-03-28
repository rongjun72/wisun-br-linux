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
TEST_TIME="0328_11-55";
####TEST_TIME=$(date "+%m%d_%H-%M");

time_start_test=$(($(date +%s%N)/1000000));
nodes_pti_cap_file=${LOG_PATH}/NodesCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
wsbrd_cap_file=${LOG_PATH}/BrCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
NodeCsvFile=${LOG_PATH}/output_node.csv;

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



####
##### TBUs config setting........
####wisun_node_set $wsnode0 $wisun_domain $wisun_mode $wisun_class
####wisun_node_set $wsnode1 $wisun_domain $wisun_mode $wisun_class
####wisun_node_set $wsnode2 $wisun_domain $wisun_mode $wisun_class
#####echo "wisun mac_allow $wsnode1_mac" > $wsnode0
#####echo "wisun mac_allow $wsnode2_mac" > $wsnode1
####
##### check the thread number of wsbrd
####ssh_check_and_kill_wsbrd $BRRPI_usr $BRRPI_ip;
####
##### get/modify/overwrite the wsbrd.conf file before start wsbrd application in RPi
####scp ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf ${LOG_PATH}/wsbrd.conf
####wisun_br_config ${LOG_PATH}/wsbrd.conf $wisun_domain $wisun_mode $wisun_class
####scp ${LOG_PATH}/wsbrd.conf ${BRRPI_usr}@${BRRPI_ip}:$BRRPI_path/wsbrd.conf
####rm -f ${LOG_PATH}/wsbrd.conf
####
####echo "----------------------------start wsbrd application on Raspberry Pi...-------"
####TIME_START_WSBRD=$(($(date +%s%N)/1000000 - $time_start_test)); # uint in ms
####TIME_START_WSBRD=$(echo "$TIME_START_WSBRD / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
#####echo "start wsbrd at time: $(($TIME_START_WSBRD/1000)).$(echo $(($TIME_START_WSBRD%1000+1000)) | sed 's/^1//')"
####echo "start wsbrd at time: $$TIME_START_WSBRD"
####ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class
####
####
####capture_time=400
####echo "----------------------------start wsnode packet capture, for ${capture_time}s---"
####echo "start wsnode packet capture, for ${capture_time}s..."
####gnome-terminal --window --title="Node Capture" --geometry=90x24+200+0 -- \
####  sudo java -jar $silabspti -ip=$wsnode0_netif,$wsnode1_netif -time=$(($capture_time*1000)) -format=pcapng_wisun -out=${nodes_pti_cap_file}
####
##### ------start wsnode join_fan10-------
####echo "----------------------------start wsnode0 join_fan10------------------------"
####time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode0 
####time_1=$(date +%s%N); echo "send wsnode0 join_fan10: $((($time_1 - $time_0)/1000000))ms";
####TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test))
####TIME_NODE0_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
#####echo "node0 start wisun join_fan10 at: $(($TIME_JOIN_FAN10/1000)).$(echo $(($TIME_JOIN_FAN10%1000+1000)) | sed 's/^1//')"
####echo "node0 start wisun join_fan10 at: $TIME_NODE0_JOIN_FAN10"
####display_wait_progress $(($capture_time/10/2));
####echo "-----------------------------------------------------------------------------"
####echo "----------------------------start wsnode1 join_fan10------------------------"
####time_0=$(date +%s%N); echo "wisun join_fan10" > $wsnode1 
####time_1=$(date +%s%N); echo "send wsnode1 join_fan10: $((($time_1 - $time_0)/1000000))ms";
####TIME_JOIN_FAN10=$(($time_1/1000000-$time_start_test));
####TIME_NODE1_JOIN_FAN10=$(echo "$TIME_JOIN_FAN10 / 1000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/'); # uint in s
####echo "node1 start wisun join_fan10 at: $TIME_NODE1_JOIN_FAN10"
####display_wait_progress $(($capture_time/10/2));
####
####
####
####
##### check session id of serial port and wsbrd(ssh RPi) and kill them
####wsbrd_id=$(ps -u | grep cd | grep 'sudo wsbrd -F' | sed 's/^[^0-9]*\([0-9]*\).*/\1/g')
####echo "kill wsbrd window: $wsbrd_id, actually wsbrd is still running on remote RPi"; kill $wsbrd_id;
##### -------------------------------------------------------------------------------------------------
##### copy border router host/rcp received message pcapng file from RapspberryPi
##### prerequiste is uncomment "pcap_file = /tmp/wisun_dump.pcapng" in wsbrd.conf in RPi
##### -------------------------------------------------------------------------------------------------
####scp ${BRRPI_usr}@${BRRPI_ip}:/tmp/wisun_dump.pcapng ${wsbrd_cap_file}
####
####


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
# :
# 3:  wpan.src64                                      22: wisun.eapol_relay.kmp_id
# 4:  wpan.dst64                                      23: wisun.eaie.eui
# 5:  frame.protocols                                 24:
# 6:  wisun.uttie.type
# 7:  eapol.type
# 8:  wlan_rsna_eapol.keydes.data
# 9:  tls.record.content_type
# 10: wlan_rsna_eapol.keydes.key_info
# 11: eapol.keydes.key_len
# 12: eap.code
# 13: eap.type
# 14: tls.handshake.type
# 15: eapol.keydes.replay_counter
# 16: wlan_rsna_eapol.keydes.nonce
# 17: eapol.keydes.key_iv
# 18: wlan_rsna_eapol.keydes.rsc
# 19: wlan_rsna_eapol.keydes.mic
# 20: wlan_rsna_eapol.keydes.data_len
# 21: wisun.eapol_relay.sup
# -------------------------------------------------------------------------------------------------
echo "------------convert Border router packet captures to csv-------------------"
# the first 2 options in EXTRACT_OPTIONS MUST be "-e frame.number -e frame.time_epoch"
EXTRACT_OPTIONS="-e frame.number -e frame.time_epoch -e wpan.src64 -e wpan.dst64 -e frame.protocols";       #1-5
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e wisun.uttie.type -e eapol.type -e wlan_rsna_eapol.keydes.data";        #6-8
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e tls.record.content_type -e wlan_rsna_eapol.keydes.key_info";           #9-10
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e eapol.keydes.key_len -e eap.code -e eap.type -e tls.handshake.type";   #11-14
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e eapol.keydes.replay_counter -e wlan_rsna_eapol.keydes.nonce";          #15-16 
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e eapol.keydes.key_iv -e wlan_rsna_eapol.keydes.rsc";                    #17-18
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e wlan_rsna_eapol.keydes.mic -e wlan_rsna_eapol.keydes.data_len";        #19-20
EXTRACT_OPTIONS="$EXTRACT_OPTIONS -e wisun.eapol_relay.sup -e wisun.eapol_relay.kmp_id -e wisun.eaie.eui";  #21-23

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
steps_pass=(); #steps_pass[0]=0;
# Step 1-2 ----
time_DUT_receive_PAS=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 5 "wpan" 6 "1"));
time_DUT_transmit_PA=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac   5 "wpan" 6 "0"));

DUT_receive_PAS_num=${#time_DUT_receive_PAS[*]};
DUT_transmit_PA_num=${#time_DUT_transmit_PA[*]};

if [ $DUT_receive_PAS_num -eq 0 ]; then
    steps_pass[1]=0;
    echo "----Step1 FAIL: DUT(BR) have not received PAS from test node"
elif [ $DUT_transmit_PA_num -eq 0 ]; then
    steps_pass[2]=0; echo "----Step2 FAIL: DUT(BR) have not send PA..."
else
    steps_pass[1]=1; echo "----Step1 PASS: DUT(BR) received PAS from TBU..."
    echo "----Step2     : DUT(BR) transmitted/broadcasted PA..."
    loop_break=0
    for pas_idx in $(seq 1 $DUT_receive_PAS_num)
    do 
        if [ $loop_break -ne 0 ]; then
          break;
        else
            #  change time precision to +/-0.1 Second
            time_pas=$(echo ${time_DUT_receive_PAS[$(($pas_idx-1))]} | sed 's/\([0-9]\+\.[0-9]\{1\}\).*/\1/' | sed 's/\.//')
            for pa_idx in $(seq 1 $DUT_transmit_PA_num)
            do 
                time_pa=$(echo ${time_DUT_transmit_PA[$(($pa_idx-1))]} | sed 's/\([0-9]\+\.[0-9]\{1\}\).*/\1/' | sed 's/\.//')
                if [ $time_pa -lt $time_pas ]; then
                    continue;
                fi
                echo "The first PA send after PAS received..$time_pa - $time_pas.."
                time_between_PA_and_PAS=$(($time_pa - $time_pas));
                echo "DUT send PA $(echo "$time_between_PA_and_PAS/10" | bc -l | sed 's/\([0-9]\+\.[0-9]\{1\}\).*/\1/')s after it receive PAS"
                ten_of_DISC_IMIN=$(($DISC_IMIN*10));
                if [ $time_between_PA_and_PAS -lt $ten_of_DISC_IMIN ]; then
                    steps_pass[2]=1; echo "----Step2 PASS: PA, PAS observed and time delta available"
                    loop_break=1;
                    break;
                else
                    steps_pass[2]=0; echo "----Step2 FAIL: PA, PAS observed, BUT too much time between PA and PAS"
                    loop_break=1;
                    break;
                fi
            done
        fi
    done
fi

# Step 3-5 ----
time_DUT_receive_EAPOLEAP=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 8 "xxxx"));
DUT_receive_EAPOLEAP=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 8 "xxxx"));
DUT_receive_EAPOLEAP_EAPOL_KEY=${DUT_receive_EAPOLEAP[7]};
DUT_receive__EAPOLEAP_num=${#time_DUT_receive_EAPOLEAP[*]};
if [[ $DUT_receive__EAPOLEAP_num -ge 1 && -n $DUT_receive_EAPOLEAP_EAPOL_KEY ]]; then
    echo "DUT_receive_EAPOLEAP_EAPOL-KEY: $DUT_receive_EAPOLEAP_EAPOL_KEY";
    steps_pass[3]=1; echo "----Step3 PASS: Joiner issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY"
else
    steps_pass[3]=0; echo "----Step3 FAIL: No EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY found"
fi

time_DUT_send_EAP_Request_Identity=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "1" 10 "1"));
DUT_send_EAP_Request_Identity=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "1" 10 "1"));
if [ -n $time_DUT_send_EAP_Request_Identity ]; then 
    echo "find DUT_send EAP Request Identity @ time: ${DUT_send_EAP_Request_Identity[1]}"
    #echo "${DUT_send_EAP_Request_Identity[@]} "
    steps_pass[4]=1; echo "----Step4 PASS: Border Router DUT sends EAP Request Identity"
else
    steps_pass[4]=0; echo "----Step4 FAIL: Border Router DUT does NOT send EAP Request Identity"
fi

time_Joiner_sends_EAP_Response_Identity=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "2" 10 "1"));
Joiner_sends_EAP_Response_Identity=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "2" 10 "1"));
if [ -n $time_Joiner_sends_EAP_Response_Identity ]; then 
    echo "find DUT_send EAP Request Identity @ time: ${Joiner_sends_EAP_Response_Identity[1]}"
    #echo "${Joiner_sends_EAP_Response_Identity[@]} "
    steps_pass[5]=1; echo "----Step5 PASS: Joiner sends EAP Response Identity"
else
    steps_pass[5]=0; echo "----Step5 FAIL: Joiner does NOT send EAP Response Identity"
fi

# Step 6-7 ----
time_DUT_sends_EAP_Request_TLS_Start=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "1" 10 "13"));
DUT_sends_EAP_Request_TLS_Start=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "1" 10 "13"));
if [ -n $time_DUT_sends_EAP_Request_TLS_Start ]; then 
    echo "find DUT sends EAP Request TLS Start @ time: ${DUT_sends_EAP_Request_TLS_Start[1]}"
    #echo "${DUT_sends_EAP_Request_TLS_Start[@]} "
    steps_pass[6]=1; echo "----Step6 PASS: Border Router DUT sends EAP Request TLS Start"
else
    steps_pass[6]=0; echo "----Step6 FAIL: Border Router DUT does NOT send EAP Request TLS Start"
fi

time_Joiner_sends_EAP_Response_Hello=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "2" 10 "13" 11 "1"));
Joiner_sends_EAP_Response_Hello=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "2" 10 "13" 11 "1"));
if [ -n $time_Joiner_sends_EAP_Response_Hello ]; then 
    echo "find Joiner_sends_EAP_Response_Hello @ time: ${Joiner_sends_EAP_Response_Hello[1]}"
    #echo "${Joiner_sends_EAP_Response_Hello[@]} "
    steps_pass[7]=1; echo "----Step7 PASS: Joiner sends EAP Response EAP-TLS Client Hello to Border Router DUT"
else
    steps_pass[7]=0; echo "----Step7 FAIL: Joiner does NOT send EAP Response EAP-TLS Client Hello to Border Router DUT"
fi

# Step 8-9 ----
time_DUT_sends_EAP_Server_Hello=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "1" 10 "13" 11 "2"));
DUT_sends_EAP_Server_Hello=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "1" 10 "13" 11 "2"));
if [ -n $time_DUT_sends_EAP_Server_Hello ]; then 
    echo "find DUT sends sends EAP-TLS: Server Hello @ time: ${DUT_sends_EAP_Server_Hello[1]}"
    #echo "${DUT_sends_EAP_Server_Hello[@]} "
    steps_pass[8]=1; echo "----Step8 PASS: Border Router DUT sends EAP-TLS: Server Hello"
else
    steps_pass[8]=0; echo "----Step8 FAIL: Border Router DUT does NOT send EAP-TLS: Server Hello"
fi

time_Joiner_sends_EAP_Response_Certificate=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "2" 10 "13" 11 "11"));
Joiner_sends_EAP_Response_Certificate=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "2" 10 "13" 11 "11"));
if [ -n $time_Joiner_sends_EAP_Response_Certificate ]; then 
    echo "find Joiner sends EAP Response Certificate, Client Key Exchange to BR DUT @ time: ${Joiner_sends_EAP_Response_Certificate[1]}"
    #echo "${Joiner_sends_EAP_Response_Certificate[@]} "
    steps_pass[9]=1; echo "----Step9 PASS: Joiner sends EAP Response EAP-TLS to BR DUT, Certificate, Client Key Exchange."
else
    steps_pass[9]=0; echo "----Step9 FAIL: Joiner does NOT send EAP Response EAP-TLS to BR DUT, Certificate, Client Key Exchange."
fi

# Step 10-12 ----
time_DUT_sends_EAP_Change_Cipher=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "1" 10 "13" 12 "20"));
DUT_sends_EAP_Change_Cipher=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol:tls" 6 "6" 7 "0" 9 "1" 10 "13" 12 "20"));
if [ -n $time_DUT_sends_EAP_Change_Cipher ]; then 
    echo "find DUT sends sends EAP-TLS: Change Cipher Spec, Finished @ time: ${DUT_sends_EAP_Change_Cipher[1]}"
    #echo "${DUT_sends_EAP_Change_Cipher[@]} "
    steps_pass[10]=1; echo "----Step10 PASS: Border Router DUT sends EAP-TLS: Change Cipher Spec, Finished"
else
    steps_pass[10]=0; echo "----Step10 FAIL: BR DUT does NOT send EAP-TLS: Change Cipher Spec, Finished"
fi

time_Joiner_sends_EAP_Response=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "2" 10 "13"));
Joiner_sends_EAP_Response=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "2" 10 "13"));
if [ -n $time_Joiner_sends_EAP_Response ]; then
    for time_item in ${time_Joiner_sends_EAP_Response[@]}; do
        greate_test=$(echo "$time_item > $time_DUT_sends_EAP_Change_Cipher" | bc -l);
        if [ "$greate_test" -eq 1 ]; then
            time_delay=$(echo "$time_item - $time_DUT_sends_EAP_Change_Cipher" | bc -l | sed 's/\([0-9]\+\.[0-9]\{6\}\).*/\1/');
            echo "Joiner sends EAP-Response ${time_delay}s after DUT send EAP-TLS: Change Cipher Spec";
            steps_pass[11]=1; echo "----Step11 PASS: Joiner sends EAP-Response after DUT send EAP-TLS: Change Cipher Spec, Finished"
            break;
        fi
    done
    #echo "${time_Joiner_sends_EAP_Response[@]} "
else
    steps_pass[11]=0; echo "----Step11 FAIL: Joiner dos NOT send EAP-Response after DUT send EAP-TLS: Change Cipher Spec, Finished"
fi

time_DUT_sends_EAP_Success=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "3"));
DUT_sends_EAP_Success=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 9 "3"));
if [ -n $time_DUT_sends_EAP_Success ]; then 
    echo "find DUT sends EAP-Success @ time: ${DUT_sends_EAP_Success[1]}"
    #echo "${DUT_sends_EAP_Success[@]} "
    steps_pass[12]=1; echo "----Step12 PASS: Border Router DUT sends EAP-Success"
else
    steps_pass[12]=0; echo "----Step12 FAIL: Border Router DUT does NOT send EAP-Success"
fi



#---------- EAP-TLS Multi-hop  PAN-KEY-TLS-8 ananlysis really start here
echo "";echo "---- EAP-TLS Multi-hop  PAN-KEY-TLS-8 ananlysis begin ----------------"
# Step 13-16 ---- ] EAP-TLS Multi-hop  analysis ---------------------------------------
time_TBD1_send_PAS=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode1_mac 5 "wpan" 6 "1"));

# Step 13: Test Bed Device E Joiner issues a EAPOL-EAP frame
time_TBD1_send_EAPOL_EAP=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode1_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 8 "xxxx"));
TBD1_send_EAPOL_EAP=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode1_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 8 "xxxx"));
#echo "time_TBD1_send_EAPOL_EAP: ${time_TBD1_send_EAPOL_EAP[@]}"
TBD1_send_EAPOL_EAP_num=${#time_TBD1_send_EAPOL_EAP[*]};
if [ $TBD1_send_EAPOL_EAP_num -ge 1 ]; then
    TBD1_send_EAPOL_EAP_KEY=${TBD1_send_EAPOL_EAP[7]};
    echo "TBD1_send_EAPOL_EAP-KEY: ${TBD1_send_EAPOL_EAP_KEY}";
    steps_pass[13]=1; echo "----Step13 PASS: TBD E Joiner issues a EAPOL-EAP frame: EAPOL-KEY Packet Type = 3 with EAPOL-KEY"
else
    steps_pass[13]=0; echo "----Step13 FAIL: TBD E Joiner does NOT send EAPOL-EAP frame, or EAPOL-KEY NOT found"
fi

# Step 14: Test Bed Device A Router sends EAPOL-RELAY
protocol_tag="wpan:6lowpan:data:ipv6:ipv6.hopopts:ipv6:udp:wisun.eapol_relay:eapol"
time_TBD0_send_EAPOL_RELAY=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 $protocol_tag 6 "4" 7 "3" 8 $TBD1_send_EAPOL_EAP_KEY));
TBD0_send_EAPOL_RELAY=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $BRRPI_mac 5 $protocol_tag 6 "4" 7 "3" 8 $TBD1_send_EAPOL_EAP_KEY));
echo "time_TBD0_send_EAPOL_RELAY: ${time_TBD0_send_EAPOL_RELAY[@]}";
TBD0_send_EAPOL_RELAY_num=${#time_TBD0_send_EAPOL_RELAY[*]};
if [ $TBD0_send_EAPOL_RELAY_num -ge 1 ]; then
    echo "find Test Bed Device A Router sends EAPOL-RELAY @ time: ${TBD0_send_EAPOL_RELAY[1]}"
    eapol_relay_sup=${TBD0_send_EAPOL_RELAY[20]};
    eapol_relay_kmp_id=${TBD0_send_EAPOL_RELAY[21]}
    steps_pass[14]=1;
    if [ "$eapol_relay_sup" != "$wsnode1_mac" ];then steps_pass[14]=0; echo "----Step14 FAIL: SUP is NOT EUI-64 of TBD E Joiner"; fi
    if [ $eapol_relay_kmp_id -ne 1 ];           then steps_pass[14]=0; echo "----Step14 FAIL: KMP ID of NOT 1"; fi

    if [ ${steps_pass[14]} -eq 1 ]; then echo "----Step14 PASS: TBD A Router sends EAPOL-RELAY with EAPOL PDU of EAPOL-KEY."; fi
else
    steps_pass[14]=0; echo "----Step14 FAIL: TBD E Joiner does NOT send EAPOL-EAP frame, or EAPOL-KEY NOT found"
fi

# Step 15: DUT sends EAP Request Identity to Test Bed Device A via EAPOL-RELAY
protocol_tag="wpan:6lowpan:ipv6:udp:wisun.eapol_relay:eapol"
time_DUT_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 $protocol_tag 6 "4" 7 "0" 12 "1" 13 "1"));
DUT_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 $protocol_tag 6 "4" 7 "0" 12 "1" 13 "1"));
echo "time_DUT_send_EAP_Request_Identity: ${time_DUT_send_EAP_REQ_ID[@]}";
DUT_send_EAP_REQ_ID_num=${#time_DUT_send_EAP_REQ_ID[*]};
if [ $DUT_send_EAP_REQ_ID_num -ge 1 ]; then
    echo "find DUT sends EAP Request Identity to TBD A @ time: ${DUT_send_EAP_REQ_ID[1]}"
    eapol_relay_sup=${DUT_send_EAP_REQ_ID[20]};
    eapol_relay_kmp_id=${DUT_send_EAP_REQ_ID[21]}
    steps_pass[15]=1;
    if [ "$eapol_relay_sup" != "$wsnode1_mac" ];then steps_pass[15]=0; echo "----Step15 FAIL: SUP is NOT EUI-64 of TBD E Joiner"; fi
    if [ $eapol_relay_kmp_id -ne 1 ];           then steps_pass[15]=0; echo "----Step15 FAIL: KMP ID of NOT 1"; fi

    if [ ${steps_pass[15]} -eq 1 ]; then echo "----Step15 PASS: DUT sends EAP Request Identity to TBD A."; fi
else
    steps_pass[15]=0; echo "----Step15 FAIL: DUT does NOT send EAP Request Identity to TBD A"
fi

# Step 16: Test Bed Device A Router sends to Joiner device Test Bed Device E via EAPOL Frame
time_TBD0_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $wsnode1_mac 5 "wpan:eapol" 6 "6" 7 "0" 12 "1" 13 "1" 23 $BRRPI_mac));
TBD0_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode0_mac 4 $wsnode1_mac 5 $protocol_tag 6 "6" 7 "0" 12 "1" 13 "1" 23 $BRRPI_mac));
echo "time_TBD0_send_EAP_Request_Identity: ${time_TBD0_send_EAP_REQ_ID[@]}";
TBD0_send_EAP_REQ_ID_num=${#time_TBD0_send_EAP_REQ_ID[*]};
if [ $TBD0_send_EAP_REQ_ID_num -ge 1 ]; then
    echo "TBD A Router sends to TBD E via EAPOL Frame @ time: ${TBD0_send_EAP_REQ_ID[1]}"
    steps_pass[16]=1; echo "----Step16 PASS: TBD A Router sends to Joiner device TBD E via EAPOL Frame";
else
    steps_pass[16]=0; echo "----Step16 FAIL: TBD A Router does NOT send to Joiner device TBD E via EAPOL Frame"
fi

# Step 17: Joiner node Test Bed Device E sends EAPOL frame to Test Bed Device A Router with EAP-Response Identity
time_TBD1_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode1_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "0" 12 "2" 13 "1"));
TBD1_send_EAP_REQ_ID=($(packet_receive_check ${NodeCsvFile} -0 3 $wsnode1_mac 4 $wsnode0_mac 5 $protocol_tag 6 "6" 7 "0" 12 "2" 13 "1"));
echo "time_TBD1_send_EAP_Request_Identity: ${time_TBD1_send_EAP_REQ_ID[@]}";
TBD1_send_EAP_REQ_ID_num=${#time_TBD1_send_EAP_REQ_ID[*]};
if [ $TBD1_send_EAP_REQ_ID_num -ge 1 ]; then
    echo "find TBD E sends EAPOL frame to TBD A Router with EAP-Response ID @ time: ${TBD1_send_EAP_REQ_ID[1]}"
    steps_pass[17]=1; echo "----Step17 PASS: TBD E sends EAPOL frame to TBD A Router with EAP-Response ID";
else
    steps_pass[17]=0; echo "----Step17 FAIL: TBD E does NOT send EAPOL frame to TBD A Router with EAP-Response ID"
fi







####
####
####
####time_GDK_DUT_ISSUE_PACKET=($(packet_receive_check ${NodeCsvFile} -t 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 13 "0x1382"));
####STEP13_DUT_ISSUE_PACKET=($(packet_receive_check ${NodeCsvFile} -0 3 $BRRPI_mac 4 $wsnode0_mac 5 "wpan:eapol" 6 "6" 7 "3" 13 "0x1382"));
####echo "time_GDK_DUT_ISSUE_PACKET: ${time_GDK_DUT_ISSUE_PACKET[@]}"
####time_DUT_1st_GDK=${time_GDK_DUT_ISSUE_PACKET[0]};
####if [ -n $time_DUT_1st_GDK ]; then 
####    echo "find DUT issues EAPOL-KEY to Joiner @ time: ${STEP13_DUT_ISSUE_PACKET[1]}"
####    ##echo "${STEP13_DUT_ISSUE_PACKET[@]} "
####    eapol_keydes_key_info=${STEP13_DUT_ISSUE_PACKET[12]};
####    eapol_keydes_key_len=${STEP13_DUT_ISSUE_PACKET[13]};
####    eapol_keydes_replay_counter=${STEP13_DUT_ISSUE_PACKET[14]};
####    eapol_keydes_nonce=${STEP13_DUT_ISSUE_PACKET[15]};
####    eapol_keydes_key_iv=${STEP13_DUT_ISSUE_PACKET[16]};
####    eapol_keydes_rsc=${STEP13_DUT_ISSUE_PACKET[17]};
####    eapol_keydes_mic=${STEP13_DUT_ISSUE_PACKET[18]};
####    eapol_keydes_data_len=${STEP13_DUT_ISSUE_PACKET[19]};
####    steps_pass[13]=1;
####    if [ "$eapol_keydes_key_info" != "0x1382" ];then steps_pass[13]=0; echo "----Step13 FAIL: Key Information: $eapol_keydes_key_info NOT 0x1382"; fi
####    if [ $eapol_keydes_key_len -ne 0 ];         then steps_pass[13]=0; echo "----Step13 FAIL: Key Length NOT 16"; fi
####    if [ $eapol_keydes_replay_counter -lt 2 ];  then steps_pass[13]=0; echo "----Step13 FAIL: Key Replay Counter LESS than 2"; fi
####    if [ ${#eapol_keydes_nonce} -eq 0 ];        then steps_pass[13]=0; echo "----Step13 FAIL: No Border Router generated ANonce found"; fi
####    if [ $eapol_keydes_key_iv -ne 0 ];          then steps_pass[13]=0; echo "----Step13 FAIL: EAPOL-Key IV NOT 0"; fi
####    if [ $eapol_keydes_rsc -ne 0 ];             then steps_pass[13]=0; echo "----Step13 FAIL: Key RSC NOT 0"; fi
####    if [ ${#eapol_keydes_mic} -eq 0 ];          then steps_pass[13]=0; echo "----Step13 FAIL: Key MIC not found"; fi
####    if [ $eapol_keydes_data_len -ne 56 ];       then steps_pass[13]=0; echo "----Step13 FAIL: Key Data Length  NOT 22"; fi
####    
####    if [ ${steps_pass[13]} -eq 1 ]; then echo "----Step13 PASS: Border Router DUT issues EAPOL-KEY with GTK to Joiner."; fi
####else
####    steps_pass[13]=0; echo "----Step13 FAIL: Border Router DUT does NOT issue EAPOL-KEY with GDK to Joiner."
####fi
####
####time_GDK_JOINER_SEND_PACKET=($(packet_receive_check ${NodeCsvFile} -t 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "3" 13 "0x0302"));
####echo "time_GDK_JOINER_SEND_PACKET: ${time_GDK_JOINER_SEND_PACKET[@]}"
##### get the index of first packet joiner send after it receive DUT(BR)'s 1st 4WH packet
##### remove items before this time 
####index_JOINER_1st_GDK=0;
####for idx in $(seq 0 ${#time_GDK_JOINER_SEND_PACKET[@]}); do
####    greate_test=$(echo "${time_GDK_JOINER_SEND_PACKET[$idx]} > $time_DUT_1st_GDK" | bc -l);
####    if [ $greate_test -eq 1 ]; then
####        time_DUT_1st_GDK=${time_GDK_JOINER_SEND_PACKET[$idx]};
####        index_JOINER_1st_GDK=$idx;
####        echo "index_JOINER_1st_GDK: $index_JOINER_1st_GDK"
####        break;
####    else
####        unset time_GDK_JOINER_SEND_PACKET[$idx];
####    fi
####done
####echo "time_GDK_JOINER_SEND_PACKET: ${time_GDK_JOINER_SEND_PACKET[@]}"
####STEP14_JOINER_SEND_PACKET=($(packet_receive_check ${NodeCsvFile} -${index_JOINER_1st_GDK} 3 $wsnode0_mac 4 $BRRPI_mac 5 "wpan:eapol" 6 "6" 7 "3" 13 "0x0302"));
####time_JOINER_1st_GDK=${time_GDK_JOINER_SEND_PACKET[$index_JOINER_1st_GDK]};
####if [ -n $time_JOINER_1st_GDK ]; then 
####    echo "find Joiner sends to BR an EAPOL-KEY0 @ time: ${STEP14_JOINER_SEND_PACKET[1]}"
####    ##echo "${STEP14_JOINER_SEND_PACKET[@]} "
####    eapol_keydes_key_info=${STEP14_JOINER_SEND_PACKET[12]};
####    eapol_keydes_key_len=${STEP14_JOINER_SEND_PACKET[13]};
####    eapol_keydes_replay_counter=${STEP14_JOINER_SEND_PACKET[14]};
####    eapol_keydes_nonce=${STEP14_JOINER_SEND_PACKET[15]};
####    eapol_keydes_key_iv=${STEP14_JOINER_SEND_PACKET[16]};
####    eapol_keydes_rsc=${STEP14_JOINER_SEND_PACKET[17]};
####    eapol_keydes_mic=${STEP14_JOINER_SEND_PACKET[18]};
####    eapol_keydes_data_len=${STEP14_JOINER_SEND_PACKET[19]};
####    steps_pass[14]=1;
####    if [ "$eapol_keydes_key_info" != "0x0302" ];then steps_pass[14]=0; echo "----Step14 FAIL: Key Information: $eapol_keydes_key_info NOT 0x0302"; fi
####    if [ $eapol_keydes_key_len -ne 0 ];         then steps_pass[14]=0; echo "----Step14 FAIL: Key Length NOT 0"; fi
####    if [ $eapol_keydes_replay_counter -lt 2 ];  then steps_pass[14]=0; echo "----Step14 FAIL: Key Replay Counter LESS than 2"; fi
####    if [ ${#eapol_keydes_nonce} -eq 0 ];        then steps_pass[14]=0; echo "----Step14 FAIL: No Border Router generated ANonce found"; fi
####    if [ $eapol_keydes_key_iv -ne 0 ];          then steps_pass[14]=0; echo "----Step14 FAIL: EAPOL-Key IV NOT 0"; fi
####    if [ $eapol_keydes_rsc -ne 0 ];             then steps_pass[14]=0; echo "----Step14 FAIL: Key RSC NOT 0"; fi
####    if [ ${#eapol_keydes_mic} -eq 0 ];          then steps_pass[14]=0; echo "----Step14 FAIL: No Key generated MIC found"; fi
####    if [ $eapol_keydes_data_len -ne 0 ];        then steps_pass[14]=0; echo "----Step14 FAIL: Key Data Length  NOT 0"; fi
####    
####    if [ ${steps_pass[14]} -eq 1 ]; then echo "----Step14 PASS: Joiner node sends to BR an EAPOL-KEY with GDK."; fi
####else
####    steps_pass[14]=0; echo "----Step14 FAIL: Joiner node does NOT send to BR an EAPOL-KEY with GDK."
####fi
####
####






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

