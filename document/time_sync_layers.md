## 1. Host与RCP之间的时间同步
Host与RCP之间是通过不断计算两者之间的时钟差别来进行的。每当Host从RCP收到特定消息时，会从中提取RCP的timestamp(微秒级)并与自己的时钟进行对比从而更新time_diff。

read_timestamp()

adjust_rcp_time_diff() in wsbr_mac.c will calculate time(in us) difference between host and RCP device.
ns_sw_mac_read_current_timestamp() get the timestamp which is amended by the adjust_rcp_time_diff()

* according to above 2 functions, host will get synchronized time/clock as the RCP device
* each time SPINEL_PROP_STREAM_STATUS/SPINEL_PROP_STREAM_RAW wsbr_mac.c will get the rcp_time_diff from messages in wsbr_spinel_is()
* 也就是在NCP/RCP->Host回复(SPINEL_PROP_STREAM_RAW)命令CMD_PROP_VALUE_IS的payload中包含着RCP/NCP的timestamp. 每次host都会根据这个更新host的时间。

### 1.1 rcp_time_diff加权迭代计算
ws_bootstrap_init(), ws_bootstrap_neighbor_set(), ws_rpl_new_parent_callback() --> ws_bootstrap_neighbor_info_request -->
ws_bootstrap_neighbor_table_clean --> ns_sw_mac_read_current_timestamp --> adjust time using adjust_rcp_time_diff()
函数：adjust_rcp_time_diff(ctxt, req.timestamp) 是一种加权趋近式算法，靠不断的更新迭代来维持一个近似的time_diff
rcp_time_diff = rcp_time_diff * 0.10 + ctxt->rcp_time_diff * 0.9


## 2. FHSS相关 

ws_bootstrap.c: ws_bootstrap_fhss_initialize() --> ns_fhss_ws_create()

通过以下函数，host会将本节点的多播/单播参数(Ch_func, 驻留间隔, 信道mask等)，父节点的广播参数和邻居节点的单播参数传递给RCP的FHSS模块.

```c
// in wsbr_fhss_mac.c
/** SPINEL_PROP_WS_FHSS_REGISTER **/
int ns_sw_mac_fhss_register(struct mac_api *mac_api, struct fhss_api *fhss_api);
/** SPINEL_PROP_WS_FHSS_UNREGISTER **/
int ns_sw_mac_fhss_unregister(struct mac_api *mac_api);
uint32_t ns_sw_mac_read_current_timestamp(struct mac_api *mac_api);
/** SPINEL_PROP_WS_ENABLE_FRAME_COUNTER_PER_KEY **/
int8_t ns_sw_mac_enable_frame_counter_per_key(struct mac_api *mac_api, bool enable_feature);

// in wsbr_fhss.net.c
/** SPINEL_PROP_WS_FHSS_CREATE **/
struct fhss_api *ns_fhss_ws_create(const struct fhss_ws_configuration *config,
                                   const fhss_timer_t *fhss_timer);
/** SPINEL_PROP_WS_FHSS_DELETE **/
int ns_fhss_delete(struct fhss_api *fhss_api);
const struct fhss_ws_configuration *ns_fhss_ws_configuration_get(const struct fhss_api *fhss_api);
/** SPINEL_PROP_WS_FHSS_SET_CONF **/
int ns_fhss_ws_configuration_set(const struct fhss_api *fhss_api,
                                 const struct fhss_ws_configuration *config);
/** SPINEL_PROP_WS_FHSS_SET_TX_ALLOWANCE_LEVEL **/
int ns_fhss_ws_set_tx_allowance_level(const fhss_api_t *fhss_api,
                                      const fhss_ws_tx_allow_level_e global_level,
                                      const fhss_ws_tx_allow_level_e ef_level);
/** SPINEL_PROP_WS_FHSS_SET_PARENT **/
int ns_fhss_ws_set_parent(const struct fhss_api *fhss_api, const uint8_t eui64[8],
                          const broadcast_timing_info_t *bc_timing_info, const bool force_synch);
/** SPINEL_PROP_WS_FHSS_UPDATE_NEIGHBOR **/
void ns_fhss_ws_update_neighbor(const uint8_t eui64[8],
                                fhss_ws_neighbor_timing_info_t *fhss_data)
/** SPINEL_PROP_WS_FHSS_DROP_NEIGHBOR **/
void ns_fhss_ws_drop_neighbor(const uint8_t eui64[8]);
int ns_fhss_set_neighbor_info_fp(const struct fhss_api *fhss_api,
                                 fhss_get_neighbor_info *get_neighbor_info);
/** SPINEL_PROP_WS_FHSS_SET_HOP_COUNT **/
int ns_fhss_ws_set_hop_count(const struct fhss_api *fhss_api, const uint8_t hop_count);
```

## 2.1 例子：
ns_fhss_ws_update_neighbor(eui64[8], *fhss_data)把邻居的单播跳频计划(clock_drift, timing_accuracy, uc_channel_list, uc_timing_info等)发给RCP中的FHSS api。

### calling topology
```c
ws_bootstrap_6lbr_asynch_ind()  ws_bootstrap_authentication_next_target()  ws_llc_mac_indication_cb()       
              |                                     |                                 |
              |                                     |                                 |
              V                                     V                                 V
    ws_bootstrap_6lbr_pan_config_analyse()    ws_bootstrap_6lbr_pan_config_solicit_analyse()
    ws_bootstrap_neighbor_set()     ws_llc_data_indication_cb()   ws_llc_eapol_indication_cb()
                  |                                                  |
                  |                                                  |
                  V                                                  V
ws_neighbor_class_neighbor_unicast_schedule_set() ws_neighbor_class_neighbor_unicast_time_info_update()
                                                |
                                                V
                                  ns_fhss_ws_update_neighbor()
                                                |
                                                V
                                            >rcp_tx()
```

## 3. 软件分层:
在wisun-br-linux中，运行了逻辑链路子层(LLC sub-layer)及以上的各层(LLC-6LowPAN-IPv6-UDP/TCP-应用)。
Host和RCP之间的通信都是Host.6LowPAN--LLC<-->RCP.MAC-SAP或FHSS之间的数据收发。
在Host上，6LowPAN通过wsbr_api.c中的mlme-sap，mcps-sap以及wsbr_fhss_mac.c, wsbr_fhss_net.c中的fhss接口向RCP传输数据。
wisun-br-linux中没有运行任何MAC层的处理。
 - app_wsbrd\wsbr_fhss_net.c 和TI的 software_stack\ti15_4mac\stack_user_api\api_mac\timac_fhss_config.c有相似的fhss api接口函数
   前者把从6LowPAN而来的fhss的相关配置通过spinel写入RCP的FHSS模块，后者把fhss配置直接通过fhss api写入FHSS模块

<img src="./wisun_ref_model.png" title="Wi-SUN" alt="Wi-SUN Logo" width="600"/>
### 3.1 Logic Link Control(LLC)子层

```c
                  // stack\source\6lowpan\ws\ws_bootstrap.c  
                  ws_bootstrap_trickle_timer()         
                      |                   |
                      |                   |
                      V                   V
  ws_bootstrap_pan_advert_solicit()    ws_bootstrap_pan_config_solicit()
  ws_bootstrap_pan_advert()            ws_bootstrap_pan_config()   
                        |                 |
                        |                 |
                        V                 V
           // stack\source\6lowpan\ws\ws_llc_data_service.c
                      ws_llc_asynch_request()
```


### 3.2 RCP上传消息
RCP上传消息有两种，一种是对host之前对下层(MAC)发出的request的回应，一种是下层自发向上发来的消息（都是异步的）。
这些消息被串口接收后，在wsbr的mainloop中会被轮询，然后执行wsbr_poll()-->rcp_rx()--SPINEL_CMD_PROP_IS-->wsbr_spinel_is()
知道被分配到mac_responce_hanlder.c中的相应响应(callback)函数。

## 4. wisun-br-linux中的spinel移植
### 4.1 可以把wisun-br-linux的spinel移植过来，包含以下文件：
 - bus_uart.c, bus_uart.h   /* 数据通过uart收发，包括crc校验, hdlc decode/encode */
 - wsbr_mac.c, wsbr_mac.h   /* rcp_tx/rcp_rx, spinel bool/byte/word/array...的读写 */
 - spinel_buffer.c, spinel_buffer.h, spinel_def.h   /* spinel buffer 的push/pop处理，spinel command,status,*properties*定义 */

### 4.2 Host与RCP之间的消息发送接收
 - wisun-br-linux中的UART数据**接收**不是基于事件进行中断处理的。这里的uart收发在background中进行，在main loop中轮询UART是否有接收数据ready。
   - UART数据接收：轮询wsbr_poll()-->rcp_rx()-->(uart_rx()--如果有接收数据-->uart_rx_hdlc()-->uart_decode_hdlc())-->spinel帧分析处理
 - 对于数据**发送**则是立即通过uart发送的.
   - spinel数据成帧-->rcp_tx()-->wsbr_uart_tx()-->uart_tx()-->uart_encode_hdlc()-->UART发送



---
# MAC sublayer上移到Host的可行性
## 1. 使能wshwsim和wsrouter的编译
在cmake时添加simulation选项：
```bash
cmake -D COMPILE_SIMULATION_TOOLS=ON -G Ninja .
```
以上选项COMPILE_SIMULATION_TOOLS在使能之后，编译生成四个可执行文件。
 - wshwsim 可以在Linux host上模拟RCP，它可以通过伪终端(串口)与wisun-br-linux相通。这个模拟的RCP通过一个socket与RF收发端相连。
 - wssimserver 模拟wisun node/br的射频端，通过socket与RCP相连。
 - wsbrd，wsnode 运行与Linux host上的Wi-SUN border router或node。可以将其与RCP相连的串口设置成伪终端(串口)。

在Linux/Ubuntu下的三个terminal中分别运行以上三个应用：
```bash
###################################################################################
# Wi-SUN br & node simulation on Linux host
# 在Linux host上可以同时运行: wsbrd + wshwsim 和 wshwsim + wsnode。 
# 前者是Wi-SUN BR包含从MAC sublayer到网络层，后者是完整的Wi-SUN node。
# 上面的"+"反映的是：两个host上运行的线程间通过伪终端(PeudoTerminal)相联系，
# 与现实中NCP到Linux host的串口/dev/ttyACMx等效。
# 两个模拟分别与BR和Node相连的NCP的wshwsim的RF端通过socket相连，模拟空中channel。
#
#                         tun0
#---------------------------------------------------------------------------------
#                   | application     |                    | application     |
#                   | RPL/ICMPv6/IPv6 |                    | RPL/ICMPv6/IPv6 |
#                   | 6LowPAN         |                    | 6LowPAN         |
#   wsbrd:          | LLC             |            wsnode: | LLC             | 
#---------------------------------------------------------------------------------
#                           |                                        |
#   PeudoTerminal       /dev/pts/m                              /dev/pts/n
#                           |                                        |
#---------------------------------------------------------------------------------
#   wshwsim:br      | MAC sublayer    |      wshwsim:node   | MAC sublayer   |
#---------------------------------------------------------------------------------
#                           |                                        |
#  wssimserver:             |____________socket_rf_driver____________|
#
###################################################################################

# run the following in sequence:  
rm -f /tmp/rf_driver /tmp/wsbr_rcp_pty /tmp/wsnode_rcp_pty; wssimserver /tmp/rf_driver
wshwsim /tmp/wsbr_rcp_pty /tmp/rf_driver -T rf,hdlc,hif
sudo wsbrd -F examples/wsbrd.conf -u /dev/pts/6 -T 15.4-mngt,15.4,eap
wshwsim /tmp/wsnode_rcp_pty /tmp/rf_driver -T rf,hdlc,hif
sudo wsnode -F examples/wsnode.conf -u /dev/pts/8 -T 15.4-mngt,15.4,eap
# each time the number /dev/pts/# is different depend on wshwsim
```

### 1.1 wssimserver的作用

```bash
#
#  BR_linux
#     |   
#  BR_RCP                                 Linux_node1
#     |                                        |
#     |                               |--- RCP_node1
#     |                               |   
#     |                               | socket[2]      Linux_node2
#     |                               |                     |
#     |                               |    |---------- RCP_node2 
#     |             socket[0]         |    | 
#     |      ====================     |    | socket[3]
#     |         -  -  -  -  -   <-----|    |                  Linux_node3
#     L------>    -  -  -  -    <----------|                       |
#  socket[1]    -  -  -  -  -   <----------------------------- RCP_node3
#            ====================        scoket[4]
#
```

wssimserver应用会创建一个用于进程间通信的socket[0]这个socket用来模拟空中信道。
这个socket会被绑定(bind)到一个本地文件，并且一直处于监听(listen)状态。
每当系统中增加了一个设备(Wi-SUN node/BR)，该设备都会连接这个socket.0，server中会由此接收并生成一个连接它的socket[i]。
当某个设备向空中发送信号时，相应的socket[j].revent事件响应中就会首先读入前面发送的信号然后把它通过socket[i](i!=j)广播给系统中其他的设备。

## 2. MAC sublayer上移的实施方法
wshwsim就是在Linux host上运行的MAC sublayer，它与运行Wi-SUN应用层、网络层的wsbrd通过伪终端相连，而且都运行在linux host上。
MAC sublayer实际上本来就已经上移了。只是目前wshwsim只是个模拟器，他与RF端并没有真实的连接。
所以上移工作应该分两步：
 - 将基于socket连接的RF driver连接到真实的串口(ttyACMx)
 - 将wsbrd与wshwsim之间的适应层(spinel-uart-spinel)去除，变成一个appl->RPL/UDP/TCP->IPv6->6LowPAN->LLC->MACsublayer的完整应用。

### 2.1 RF_driver连接真实射频前端
```bash
wshwsim /tmp/wsbr_rcp_pty /tmp/rf_driver
```
上面第二个参数是：socket_open(const char *path)的输入参数，这使得wshwsim的射频端与wssimserver的socket相连。
把socket_open改为：pty_open(const char *dest_path, int bitrate, bool hardflow)可以将RF端与实际串口(或伪终端)相连。
另外，我们需要通过串口连接的射频前端。这个可以通过Silabs的RAIL实现。

**问题**：
 - Host上运行的sl_wsrcp与射频前端通过串口连接，**传输时延**如何考虑？传输需要差错控制吗？
 - FHSS如何实现？RCP中FHHS接收上层传来的广播和单播的确定参数(BDI, UDI, 广播、单播计划，time_stamp), 在每个数据帧发送接收时
   按上面的参数确定当前帧发送或接收的时间和信道。 

mac_mlme_rf_channel_change()
phy_rf_tx()中会组织好PHY帧，其帧结构："xx" + data_len + channel + data(data length = data_len)
一个完整的帧的发送分成连续的两次发送：
 - 1. 帧头发送："xx" + data_len + channel
 - 2. payload发送： data with length = data_len
 
rf_init(void), nanostack_rf_phy_cmt2310a_init()
in mbed-os\connectivity\drivers\802.15.4_RF\cmostek-cmt2310a-rf-driver\source\nanostack_rf_phy_cmt2310a.c

rf_pin_init(rf_pin_t* interface, PinName spi_sdi, PinName spi_sdo, PinName spi_sclk, PinName spi_cs, PinName spi_sdn, PinName irq_pin, PinName gpio1)

CMT2310A作为Wi-SUN BR的射频前端与国民技术的MCU EV board通过4-wire SPI和几个GPIO相连。CMT2310A主要实现15.4PHY功能，包括CSMA、自动跳频收发、RSSI估计等。
Linux Host 只能通过ttyACMx方式与外部设备相连，无法向国民技术EVB+CMT2310A那样连接。Linux host还是要通过串口(需要spinel)与EVB相连而EVB上挂载RF前端。
MCU只需要完成PHY-SAP的工作。
```c
    cmt2310a_phy_interface->_spi_sdi   = CMT2310A_SPI_SDI;
    cmt2310a_phy_interface->_spi_sdo   = CMT2310A_SPI_SDO;
    cmt2310a_phy_interface->_spi_sclk  = CMT2310A_SPI_SCLK;
    cmt2310a_phy_interface->_spi_cs    = CMT2310A_SPI_CS;
    cmt2310a_phy_interface->_spi_sdn   = CMT2310A_SPI_SDN;
    cmt2310a_phy_interface->_spi_gpio0 = CMT2310A_SPI_GPIO0;
    cmt2310a_phy_interface->_spi_gpio1 = CMT2310A_SPI_GPIO1;
    cmt2310a_phy_interface->_spi_gpio2 = CMT2310A_SPI_GPIO2;
    cmt2310a_phy_interface->_spi_gpio3 = CMT2310A_SPI_GPIO3;
```