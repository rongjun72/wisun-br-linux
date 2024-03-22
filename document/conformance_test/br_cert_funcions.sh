

# --------------------------------------------------------------------------------------------
# Function definations:
# --------------------------------------------------------------------------------------------

function wisun_node_set
{
  # Input parameters:
  # ---------------------------------------------
  # $1: serial port device name of Wi-SUN node
  # $2: expected Wi-SUN domain, like NA,EU,CN
  # $3: expected Wi-SUN mode
  # $4: expected Wi-SUN class
  #----------------------------------------------
  local seria_port=$1
  local wisun_domain=$2
  local wisun_mode=$3
  local wisun_class=$4
  echo "wisun disconnect" > $seria_port
  #echo "wisun get wisun" > $seria_port
  #echo "wisun set wisun.network_name \"WiSUN test\"" > $seria_port
  sleep 0.2; echo "wisun set wisun.regulatory_domain $wisun_domain" > $seria_port 
  sleep 0.2; echo "wisun set wisun.operating_class $wisun_class" > $seria_port
  sleep 0.2; echo "wisun set wisun.operating_mode $wisun_mode" > $seria_port
  sleep 0.2; echo "wisun set wisun.unicast_dwell_interval 15" > $seria_port
  #sleep 0.2; echo "wisun set wisun.allowed_channels 0-255" > $seria_port
  # Channel Function for both devices are set to Fixed Channel
  sleep 0.2; echo "wisun set wisun.allowed_channels 0" > $seria_port
  # clear the white list before set the list
  sleep 0.2; echo "wisun mac_allow FF:FF:FF:FF:FF:FF:FF:FF" > $seria_port
  sleep 0.2; echo "wisun save" > $seria_port
  sleep 0.5
}

function wisun_br_config
{
  # Input parameters:
  # -------------------------------------------------
  # $1: wsbrd.conf file to be modified
  # $2: expected Wi-SUN domain
  # $3: expected Wi-SUN mode
  # $4: expected Wi-SUN class
  #--------------------------------------------------
  local Fwsbrd=$1
  local wisun_domain=$2
  local wisun_mode=$3
  local wisun_class=$4
  local gdk0=""

  # find "domain = XX" and replace with "domain = YY #XX", mean while delete possible comment symbol #
  sed -i "s/^.*domain =/domain = $wisun_domain #/" $Fwsbrd
  sed -i "s/^.*mode =/mode = $wisun_mode #/" $Fwsbrd
  sed -i "s/^.*class =/class = $wisun_class #/" $Fwsbrd
  sed -i "s/^.*unicast_dwell_interval =/unicast_dwell_interval = 15 #/" $Fwsbrd
  # Channel Function for both devices are set to Fixed Channel 0
  sed -i "s/^.*allowed_channels =/allowed_channels = 0 #/" $Fwsbrd
  # set expected GTKs 
  #sed -i "s/^.*gtk[0] =/gtk[0] =/" $Fwsbrd
}


function join_fan10
{
  local seria_port=$1
  echo "wisun join_fan10" > $seria_port
}

function ssh_check_and_kill_wsbrd
{
  # Input parameters:
  # ---------------------------------------------
  # $1: usr name of border router Raspberry Pi
  # $2: ip address of border router Raspberry Pi
  #----------------------------------------------
  local BRRPI_usr=$1
  local BRRPI_ip=$2

  # check the thread number of wsbrd
  # return of "ps -u wsbrd" is "PID TTY TIME CMD $pid_val ? hour:min:sec wsbrd"
  # here we extact the thread id : $pid_val, if existed, kill it
  local thread=$(echo $(ssh ${BRRPI_usr}@${BRRPI_ip} "ps -u wsbrd") | sed 's/^.*CMD //g' | sed 's/ .*$//g')
  echo "------check the thread number of wsbrd: $thread"
  # kill existing wsbrd thread before start
  if [ -n "$thread" ]; then
    echo "------kill the existing wsbrd thread: $thread before start" 
    ssh ${BRRPI_usr}@${BRRPI_ip} "sudo kill $thread";
  fi
}

function ssh_start_wsbrd_window
{
  # Input parameters:
  # ---------------------------------------------
  # $1: usr name of border router Raspberry Pi
  # $2: ip address of border router Raspberry Pi
  # $3: expected Wi-SUN domain, like NA,EU,CN
  # $4: expected Wi-SUN mode
  # $5: expected Wi-SUN class
  #----------------------------------------------
  local BRRPI_usr=$1
  local BRRPI_ip=$2
  local wisun_domain=$3
  local wisun_mode=$4
  local wisun_class=$5
  local trace_set="15.4-mngt,15.4,eap,icmp-rf"

  gnome-terminal --window --title="BR log" --geometry=110x45+0+0 -- ssh ${BRRPI_usr}@${BRRPI_ip} \
    "cd $BRRPI_path;sudo wsbrd -F wsbrd.conf -D -d $3 -m $4 -c $5 -T $trace_set"
  sleep 0.5;  
}

function find_netif_for_pti_capture 
{
  # Input parameters:
  # -----------------------------------------
  # $1: si_pti_discover.txt 
  # $2: serial number of fg12 radio board
  #------------------------------------------
  local wsnode_netif=""

  local line_sn=$(sed -n '/'$2'/=' $1)
  if [ -n "$line_sn" ]; then
    #echo "node jlink sn found in line: $line_sn..."
    local line_ip=$(($line_sn + 1)); 
    wsnode_netif=$(sed -n "${line_ip}p" $1 | sed 's/^.*netif=//g')
    #echo "node net if is: $wsnode_netif"
  fi
  echo $wsnode_netif
}

function display_wait_progress
{
  # Input parameters:
  # -----------------------------------------
  # $1: wait time in second 
  #------------------------------------------
  local wait_sec=$1
  for wait_idx in $(seq 0 $wait_sec)
  do
    #echo $(date)
    echo -ne "\r"
    for idx in $(seq 0 $wait_idx)
    do
      if [ "$idx" = "0" ]; then
        echo -ne "\r."
      else
        echo -n "."
      fi
    done
    for idx in $(seq 0 $(($wait_sec - $wait_idx)))
    do
      echo -n "|"
    done
    sleep 10
  done
  echo -ne "\n"
}

function compensate_node_csv_time
{
  # function description:
  # compensate time in node router captue cav file
  # add estimated average timeoffset. 
  # 
  # Assuming: the first 2 options in EXTRACT_OPTIONS MUST be "-e frame.number -e frame.time_epoch"
  # -----------------------------------------------------------------------------
  # Input parameters:
  # -----------------------------------------
  # $1: estimated average timeoffset 
  # $2: node router capture csv file 
  #------------------------------------------
  local time_offset=$1
  local NodeCsv=$2

  time_offset=$(echo "$time_offset / 1000 + 0.000" | bc -l | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/')
  echo "compensate time_offset: $time_offset, according to Br capture time"

  for line_idx in $(seq 1 $(sed -n '$=' $NodeCsv))
  do
    time_origin=$(sed -n "${line_idx}p" $NodeCsv | cut -f 2);
    time_compensate=$(echo "$time_origin+$time_offset" | bc -l | sed 's/\([0-9]\+\.[0-9]\{9\}\).*/\1/');
    #echo "line: $line_idx -- $time_origin + $time_offset = $time_compensate"
    sed -i "${line_idx}s/[0-9]\+\.[0-9]\+/${time_compensate}/" $NodeCsv;
  done
}

function synchronize_node_cap_to_Br_cap
{
  # function description:
  # time in origin node router captue file is relative time to the first packet.
  # the purpose of this function is try to synchronize time of router capture
  # to border router capture time. 
  # 
  # Assuming: the first 2 options in EXTRACT_OPTIONS MUST be "-e frame.number -e frame.time_epoch"
  # -----------------------------------------------------------------------------
  # Input parameters:
  # -----------------------------------------
  # $1: border router capture csv file 
  # $2: node router capture csv file
  #------------------------------------------
  local BrCsv=$1;
  local NodeCsv=$2;
  local MAX_FAILs=20;
  local fail_cnt=0;
  local SUCCESS_HITs=120;
  local hit_cnt=0;
  local br_search_line=0
  local node_search_line=0
  local COL_START=3
  local total_time_offset_avg=0
  local total_time_offset_cnt=0
  local VAR_THRESHOLD=1000

  # while [ $fail_cnt le $MAX_FAILs ]
  # COL_START=3;line=3;sss=$(cat output_br.csv | sed -n "${line}p" | cut -f ${COL_START}-);echo $sss
  # sed -n "/${sss}/=" output_node.csv
  for br_line in $(seq 1 $(sed -n '$=' $BrCsv))
  do
    # extract line:$(br_line) from BR file for later searching
    search_str=$(cat $BrCsv | sed -n "${br_line}p" | cut -f ${COL_START}-);

    # for the given string,find lines that the string exist in $BrCsv
    #found_matrix_br=$(sed -n "/${search_str}/p" $BrCsv);
    time_array_br=($(sed -n "/${search_str}/p" $BrCsv | cut -f 2 | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/' | sed 's/\.//g'));
    found_lines_br=$(sed -n "/${search_str}/=" $BrCsv | sed -n '$=');
    # for the given string,find lines that the string exist in $NodeCsv
    #found_matrix_node=$(sed -n "/${search_str}/p" $NodeCsv);
    time_array_node=($(sed -n "/${search_str}/p" $NodeCsv | cut -f 2 | sed 's/\([0-9]\+\.[0-9]\{3\}\).*/\1/' | sed 's/\.//g'));
    found_lines_node=$(sed -n "/${search_str}/=" $NodeCsv | sed -n '$=');
    time_offset=("");
    #echo "[$found_lines_br == $found_lines_node]"
    # calculate time offset between BrCsv and NodeCsv
    if [ $found_lines_br == $found_lines_node ] && [ $found_lines_br -ne 1 ]; then
      time_offset_avg=0;
      time_offset_var=0;
      # calculate time offset average
      for idx in $(seq 1 $found_lines_br)
      do
        arr_idx=$(($idx-1));
        time_offset[$arr_idx]=$((${time_array_br[$arr_idx]}-${time_array_node[$arr_idx]}));
        time_offset_avg=$(($time_offset_avg + ${time_offset[$arr_idx]}/$found_lines_br))
      done
      # calculate time offset variance
      for idx in $(seq 1 $found_lines_br)
      do
        arr_idx=$(($idx-1));
        time_offset_var=$((  $time_offset_var + (${time_offset[$arr_idx]}-$time_offset_avg)*(${time_offset[$arr_idx]}-$time_offset_avg)/$found_lines_br  ))
      done
      #echo "time offset : ${time_offset[*]}"
      #echo "time offset average: $time_offset_avg"
      #echo "time offset variance: $time_offset_var"


      if [ $time_offset_var -lt $VAR_THRESHOLD ]; then
        hit_cnt=$(($hit_cnt + $found_lines_br));
        #echo "hit count increase: $hit_cnt"
        total_time_offset_avg=$(($total_time_offset_avg*($hit_cnt-$found_lines_br) + $time_offset_avg*$found_lines_br));
        total_time_offset_avg=$(($total_time_offset_avg/$hit_cnt));
      fi
      if [ $hit_cnt -gt $SUCCESS_HITs ]; then
        #echo " $hit_cnt > $SUCCESS_HITs"
        echo "Find time offset average esitmate: $total_time_offset_avg"
        compensate_node_csv_time  $total_time_offset_avg $NodeCsv
        break;
      fi
    fi
  done
  if [ $hit_cnt -lt 5 ]; then
    echo "can not synchronize time for $BrCsv and $NodeCsv "
  fi
}

function check_receive_message
{
  # function description:
  # check if given device received given packet
  # 
  # -----------------------------------------------------------------------------
  # Input parameters:
  # ----------------------------------------------
  # $1: packet capture file 
  # $2: packet come from (source mac address) 
  # $3: packet protocol
  # $4: wisun.uttie.type 
  #-----------------------------------------------
  # Return:
  # ----------------------------------------------
  # : if exist, return the packet receiving times 
  #-----------------------------------------------
  local CaptureCsv=$1;
  local source_mac=$2
  local dest_mac=$3
  local protocol=$4
  local wisun_uttie_type=$5

  # source_mac=$(cat output_node.csv | sed -n "5p" | cut -f 3)
  #check_item=$(sed -n "/${source_mac}\t${dest_mac}\t${protocol}\t1/p" ${CaptureCsv})
  check_item=$(sed -n "/${source_mac}\t${dest_mac}\t${protocol}\t${wisun_uttie_type}/p" ${CaptureCsv})
  check_item_num=$(sed -n "/${source_mac}\t${dest_mac}\t${protocol}\t${wisun_uttie_type}/p" ${CaptureCsv} | sed -n '$=')
  
  #echo "check_item = $check_item"
  #echo "check_item_num = $check_item_num"
  
  if [ -n $check_item_num ]; then
    echo $(sed -n "/${source_mac}\t${dest_mac}\t${protocol}\t${wisun_uttie_type}/p" ${CaptureCsv} | cut -f 2)
  fi

}

function packet_receive_check
{
  # function description:
  # check if given key words exit in given csv file
  # 
  # -----------------------------------------------------------------------------
  # Input parameters:
  # ----------------------------------------------
  # $@: variable input sequences  
  # $1: the 1st parameter MUST be given csv file
  #   : parameters after MUST be paired like:
  #     [collum number, key word]
  #-----------------------------------------------
  # Return:
  # ----------------------------------------------
  # : if exist, return the packet receiving times 
  #-----------------------------------------------
  local CaptureCsv=$1;

  #echo "number of input parameter: $#"

  # for tmp in ${sss[@]}; do echo $tmp >> temp.txt ; done; cat temp.txt | sort | uniq -d
  # create a temperary file for search ,sort and repeated check
  CURR_TIME=$(date "+%m%d_%H-%M");
  for idx in $(seq 0 $#); do
    rm -rf temp_${CURR_TIME}_${idx}.txt
  done

  # search for each input param: [col, key_word]
  for idx in $(seq 2 2 $#); do
    search_col=$(eval echo \$${idx}); search_key=$(eval echo \$$(($idx+1)));
    #echo "[collum, key_word] : $search_col , $search_key"
    #cat ${CaptureCsv} | cut -f $search_col | sed -n "/${search_key}/p"
    #cat ${CaptureCsv} | cut -f $search_col | sed -n "/${search_key}/="
    searched_lines=($(cat ${CaptureCsv} | cut -f $search_col | sed -n "/${search_key}/="));
    for tmp in ${searched_lines[@]}; do 
      echo $tmp >> temp_${CURR_TIME}_${idx}.txt; 
    done

    # paste the searched lines to temperary files
    # for the first 
    if [ $idx -eq 2 ]; then
      cp temp_${CURR_TIME}_${idx}.txt temp_${CURR_TIME}_$(($idx+1)).txt;
    else
      sort -n temp_${CURR_TIME}_${idx}.txt temp_${CURR_TIME}_$(($idx-1)).txt | uniq -d > temp_${CURR_TIME}_$(($idx+1)).txt;
    fi
  done

  #echo "--------------------"
  #cat temp_${CURR_TIME}_$(($#)).txt
  #echo "--------------------"

  # convert searched indice to array
  items=($(cat temp_${CURR_TIME}_$(($#)).txt));
  items_num=${#items[*]}; items_num=$(($items_num-1))
  for idx in $(seq 0 $items_num); do
    line_idx=${items[$idx]};
    result[$idx]=$(cat ${CaptureCsv} | sed -n "${items[$idx]}p" | cut -f 2);
  done
  echo ${result[@]}

  # remove temperary files
  for idx in $(seq 0 $#); do
    rm -rf temp_${CURR_TIME}_${idx}.txt
  done
} 