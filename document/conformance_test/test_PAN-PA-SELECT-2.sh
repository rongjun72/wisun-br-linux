source br_cert_funcions.sh
# -----------------------------------------------------------------------
# Test case :   Border Router as DUT [PAN-PA-SELECT-2]
# -----------------------------------------------------------------------
# Description:  
# This test case verifies the ability of the Border Router (DUT) to be 
# configured with a PAN ID for PAN A and respond to PAN Advertisement 
# Solicit frames from a joining node.
# -----------------------------------------------------------------------
TEST_CASE_NAME="PAN-PA-SELECT-2"



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
TEST_TIME=$(date "+%m%d_%H-%M");
time_start_test=$(($(date +%s%N)/1000000)); 
node0_pti_cap_file=${LOG_PATH}/Node0Cap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng
wsbrd_cap_file=${LOG_PATH}/BrCap_${TEST_CASE_NAME}_${TEST_TIME}.pcapng

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
TIME_START_WSBRD=$(($(date +%s%N)/1000000 - $time_start_test)) # uint in ms
echo "start wsbrd at time: $(($TIME_START_WSBRD/1000)).$(echo $(($TIME_START_WSBRD%1000+1000)) | sed 's/^1//')"
ssh_start_wsbrd_window $BRRPI_usr $BRRPI_ip $wisun_domain $wisun_mode $wisun_class


capture_time=200
echo "----------------------------start wsnode packet capture, for ${capture_time}s---"
echo "start wsnode packet capture, for ${capture_time}s..."
gnome-terminal --window --title="Node Capture" --geometry=90x24+200+0 -- \
  sudo java -jar $silabspti -ip=$wsnode0_netif -time=$(($capture_time*1000)) -format=pcapng_wisun -out=${node0_pti_cap_file}

# ------start wsnode join_fan10-------
echo "----------------------------start wsnode0 join_fan10------------------------"
time_0=$(date +%s%N); echo "wisun disconnect" > $wsnode0 
time_1=$(date +%s%N); echo "send disconnect: $((($time_1 - $time_0)/1000000))ms"; echo "wisun join_fan10" > $wsnode0 
time_2=$(date +%s%N); echo "send join_fan10: $((($time_2 - $time_1)/1000000))ms";
TIME_JOIN_FAN10=$(($time_2/1000000-$time_start_test))
echo "node0 start wsnode join_fan10 at: $(($TIME_JOIN_FAN10/1000)).$(echo $(($TIME_JOIN_FAN10%1000+1000)) | sed 's/^1//')"
echo "-----------------------------------------------------------------------------"




display_wait_progress $(($capture_time/10));
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
# ------- post test analysis begin based on ${wsbrd_cap_file} and ${node0_pti_cap_file}
# -------------------------------------------------------------------------------------------------
# wisun.uttie.type: "Frame Type: PAN Advertisement (0)"           - PA
#                   "Frame Type: PAN Advertisement Solicit (1)"   - PAS
#                   "Frame Type: PAN Configuration (2)"           - PC
#                   "Frame Type: PAN Configuration Solicit (3)"   - PCS
#                   "Frame Type: Data (4)"                        - Data
#                   "Frame Type: Acknowledgment (5)"              - ACK
#                   "Frame Type: EAPOL (6)"                       - EAPOL
echo "------------convert Border router packet captures to csv-------------------"
# the first 2 options in EXTRACT_OPTIONS MUST be "-e frame.number -e frame.time_epoch"
EXTRACT_OPTIONS="-e frame.number -e frame.time_epoch -e wpan.src64 -e wpan.dst64 -e frame.protocols -e wisun.uttie.type"
tshark -r ${wsbrd_cap_file} -T fields $EXTRACT_OPTIONS > ${LOG_PATH}/output_br.csv
tshark_mod ${LOG_PATH}/output_br.csv  ${LOG_PATH}/output_br.csv


echo "-------------convert Node router packet captures to csv---------------------"
# text2pcap -q -t %H:%M:%S. -l 230 -n Node0Cap_PAN-PA-SELECT-2_0314_18-12.txt Node0Cap_PAN-PA-SELECT-2_0314_18-12.pcapng
# -n : output file format of pcapng not default pcap format
# -l : link-layer type number; default is 1 (Ethernet). see https://www.tcpdump.org/linktypes.html for other
tshark -r ${node0_pti_cap_file} -T fields $EXTRACT_OPTIONS > ${LOG_PATH}/output_node.csv
tshark_mod ${LOG_PATH}/output_node.csv  ${LOG_PATH}/output_node.csv

synchronize_node_cap_to_Br_cap ${LOG_PATH}/output_br.csv ${LOG_PATH}/output_node.csv

# [PAN-PA-SELECT-2] Pass/Fail Criteria
# -------------------------------------------------------------------------------------------------
# Step2 PASS: Wireshark capture shows transmission of PA from DUT before DISC_IMIN time passes after receiving the PAS.
#       FAIL: PAN Advertisement frame fails to be transmitted from DUT within DISC_IMIN seconds of the PAS being received.
# -------------------------------------------------------------------------------------------------
# Step3 PASS: DUT Border Router PAN Advertisement frame observed
#       FAIL: No DUT Border Router PAN Advertisement frames observed
# -------------------------------------------------------------------------------------------------
time_DUT_receive_PAS=($(packet_receive_check ${LOG_PATH}/output_node.csv -t 3 $wsnode0_mac 5 "wpan" 6 "1"));
time_DUT_transmit_PA=($(packet_receive_check ${LOG_PATH}/output_node.csv -t 3 $BRRPI_mac   5 "wpan" 6 "0"));

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


echo "----TEST [$TEST_CASE_NAME] complete .............................................................."
echo ""
echo ""

