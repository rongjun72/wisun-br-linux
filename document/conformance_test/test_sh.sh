LOG_PATH=$(pwd)/log
WisunBR_PATH="~/Git_repository/wisun-br-rong"

BorderRouterRPi_usr="cmt"
BorderRouterRPi_ip="192.168.31.97"

# check serial port discripter for your test bed
wscli0=/dev/ttyACM0
wscli1=/dev/ttyACM1
wscli2=/dev/ttyACM2
# get access permission to serial ports
sudo chmod 777 $wscli0
sudo chmod 777 $wscli1
sudo chmod 777 $wscli2
# might not needed
nohup minicom -D $wscli0 &
# serial port config
stty -F ${wscli0} ispeed 115200 ospeed 115200 -parenb cs8 -cstopb




function wisun_set
{
    echo "$1 is not set, exiting"
}

wisun_set $wscli0