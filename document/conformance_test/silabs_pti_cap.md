## wireshark extarnal capture
craete a soft link in /root/.config/wireshark that points to wireshark extcap folder: 
    /usr/lib/x86_64-linux-gnu/wireshark/extcap
In this folder there will be a external capture interface for silabs PTI
```bash
# open a terminal by gnome-terminal 
# super administrator
su
# goto /root
cd ~
mkdir .config
mkdir -p .config/wireshark
ln -s /usr/lib/x86_64-linux-gnu/wireshark/extcap .config/wireshark/extcap

```

## Silabs packet trace library build
Silabs-pti is a java based client side library, used to communicate with Silicon Labs WSTK adapters and stream debug channel messages to a file.
This includes network packet data or any other data available over the debug channel.
Data can be saved in a PCAPNG format to be consumed by wireshark, Silicon Labs Network Analyzer, or as plain text or binary files.
It is also suitable for direct integration with the Wireshark extcap interface, so you can capture directly from Wireshark.
Get github clone from: https://github.com/SiliconLabs/java_packet_trace_library.git
*Note:* There is issue in local javadoc build. We have to modify file: 
    ../Git_repository/java_packet_trace_library/silabs-pti/build.gradle  
Delete item: *javadocJar* in: task all {}, the install openjdk-18-jre-headless can not build doc related
Then, we run bash as below:

```bash
# prerequisite: wireshark and tshark install
sudo apt install wireshark
sudo apt install tshark
sudo apt install openjdk-18-jre-headless -y
cd /home/jurong/Git_repository/java_packet_trace_library
sudo ./gradlew all
sudo ./gradlew wireshark

```

```bash
# to run silabs-pti external capture, we need superviser
sudo wireshark

java -jar ../java_packet_trace_library/silabs-pti/build/libs/silabs-pti-1.12.2.jar -discover

# check available interfaces, for silabs-pti, "192.168.31.160 (Silicon Labs WSTK adapter)"
sudo tshark -D
# command line start wireshark silabs-pti capture, where 192.168.31.160 is the ip of WSTK
sudo wireshark -k -i 192.168.31.160

silabspti=/home/jurong/Git_repository/java_packet_trace_library/silabs-pti/build/libs/silabs-pti-1.12.2.jar

# command line start Silabs-pti capture to a file that Network Analyzer can import
sudo java -jar $silabspti -ip=192.168.31.160 -time=50000 -format=log -out=test_cap.log

# command line start Silabs-pti capture to a file that can be used with wireshark by
# running through 'text2pcap -q -t %H:%M:%S. test_cap.text test_cap.pcapng'
# !!Curently, there is issue that wireshark can not decode the packets captured properly
sudo java -jar $silabspti -ip=192.168.31.160 -time=50000 -driftCorrection=disable -format=text -out=test_cap.text
text2pcap -q -t %H:%M:%S. test_cap.text test_cap.pcapng
```