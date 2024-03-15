

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
