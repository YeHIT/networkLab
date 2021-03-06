#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

#define MAX_BUF_NUMBER 16
/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type = swap16(ARP_HW_ETHER),
    .pro_type = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = DRIVER_IF_IP,
    .sender_mac = DRIVER_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表
 * 
 */
arp_entry_t arp_table[ARP_MAX_ENTRY];

/**
 * @brief 长度为1的arp分组队列，当等待arp回复时暂存未发送的数据包
 * 
 */
arp_buf_t arp_buf[MAX_BUF_NUMBER];

/**
 * @brief 更新arp表
 *        你首先需要依次轮询检测ARP表中所有的ARP表项是否有超时，如果有超时，则将该表项的状态改为无效。
 *        接着，查看ARP表是否有无效的表项，如果有，则将arp_update()函数传递进来的新的IP、MAC信息插入到表中，
 *        并记录超时时间，更改表项的状态为有效。
 *        如果ARP表中没有无效的表项，则找到超时时间最长的一条表项，
 *        将arp_update()函数传递进来的新的IP、MAC信息替换该表项，并记录超时时间，设置表项的状态为有效。
 * 
 * @param ip ip地址
 * @param mac mac地址
 * @param state 表项的状态
 */
void arp_update(uint8_t *ip, uint8_t *mac, arp_state_t state)
{
    // TODO
    int maxIndex = 0;
    time_t now_time = time(&now_time);
    double maxTimeApart = difftime(now_time,arp_table[0].timeout);
    //查看是否超时
    for (int i = 0; i < ARP_MAX_ENTRY; i++){
        double timeApart = difftime(now_time,arp_table[i].timeout);
        //超时则置为ARP_INVALID
        if(timeApart > ARP_TIMEOUT_SEC){
            arp_table[i].state = ARP_INVALID;
        } 
        //当前超时时间大于已知最大超时时间，最大值下标更改
        if(timeApart > maxTimeApart){
            maxIndex = i;
            maxTimeApart = timeApart;
        }
    }
    //查看是否存在ARP_INVALID，若存在则直接使用
    for (int i = 0; i < ARP_MAX_ENTRY; i++){
        if(arp_table[i].state == ARP_INVALID){
            memcpy(arp_table[i].ip,ip,NET_IP_LEN);
            memcpy(arp_table[i].mac,mac,NET_MAC_LEN);
            arp_table[i].state = state;
            arp_table[i].timeout = now_time;
            return;
        }
    }
    //将超时时间最长的表项替换
    memcpy(arp_table[maxIndex].ip,ip,NET_IP_LEN);
    memcpy(arp_table[maxIndex].mac,mac,NET_MAC_LEN);
    arp_table[maxIndex].state = state;
    arp_table[maxIndex].timeout = now_time;
    return;
}

/**
 * @brief 从arp表中根据ip地址查找mac地址
 * 
 * @param ip 欲转换的ip地址
 * @return uint8_t* mac地址，未找到时为NULL
 */
static uint8_t *arp_lookup(uint8_t *ip)
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        if (arp_table[i].state == ARP_VALID && memcmp(arp_table[i].ip, ip, NET_IP_LEN) == 0)
            return arp_table[i].mac;
    return NULL;
}

/**
 * @brief 发送一个arp请求
 *        你需要调用buf_init对txbuf进行初始化
 *        填写ARP报头，将ARP的opcode设置为ARP_REQUEST，注意大小端转换
 *        将ARP数据报发送到ethernet层
 * 
 * @param target_ip 想要知道的目标的ip地址
 */
static void arp_req(uint8_t *target_ip)
{
    // TODO
    net_protocol_t protocol = NET_PROTOCOL_ARP;
    //以太网帧头MAC应为全1。
    uint8_t eth_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //ARP报文中目的MAC应为全0。
    uint8_t arp_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf_init(&txbuf,28);
    arp_pkt_t *arp_pkt = (arp_pkt_t *) txbuf.data;
    //初始化ARP报头
    arp_pkt->hw_type = swap16(ARP_HW_ETHER);
    arp_pkt->pro_type = swap16(NET_PROTOCOL_IP) ;
    arp_pkt->hw_len = NET_MAC_LEN;
    arp_pkt->pro_len = NET_IP_LEN;
    arp_pkt->opcode = swap16(ARP_REQUEST);
    //发送方ip和mac
    memcpy(arp_pkt->sender_ip,net_if_ip,NET_IP_LEN);
    memcpy(arp_pkt->sender_mac,net_if_mac,NET_MAC_LEN);
    //接收方ip和mac
    memcpy(arp_pkt->target_ip,target_ip,NET_IP_LEN);
    memcpy(arp_pkt->target_mac,arp_mac,NET_MAC_LEN);
    //发送ARP报文
    ethernet_out(&txbuf,eth_mac,protocol);
}

/**
 * @brief 发送一个arp响应
 * 
 * @param dst_mac 目的mac
 * @param dst_mac 目的ip
 */
static void arp_reply(uint8_t *dst_mac,uint8_t *dst_ip){
    // TODO
    net_protocol_t protocol = NET_PROTOCOL_ARP;
    buf_init(&txbuf,28);
    arp_pkt_t *arp_pkt = (arp_pkt_t *) txbuf.data;
    //初始化ARP报头
    arp_pkt->hw_type = swap16(ARP_HW_ETHER);
    arp_pkt->pro_type = swap16(NET_PROTOCOL_IP) ;
    arp_pkt->hw_len = NET_MAC_LEN;
    arp_pkt->pro_len = NET_IP_LEN;
    arp_pkt->opcode = swap16(ARP_REPLY);
    //发送方ip和mac
    memcpy(arp_pkt->sender_ip,net_if_ip,NET_IP_LEN);
    memcpy(arp_pkt->sender_mac,net_if_mac,NET_MAC_LEN);
    //接收方ip和mac
    memcpy(arp_pkt->target_ip,dst_ip,NET_IP_LEN);
    memcpy(arp_pkt->target_mac,dst_mac,NET_MAC_LEN);
    //发送ARP reply报文
    ethernet_out(&txbuf,dst_mac,protocol);
}


/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，查看报文是否完整，
 *        检查项包括：硬件类型，协议类型，硬件地址长度，协议地址长度，操作类型
 *        
 *        接着，调用arp_update更新ARP表项
 *        查看arp_buf是否有效，如果有效，则说明ARP分组队列里面有待发送的数据包。
 *        即上一次调用arp_out()发送来自IP层的数据包时，由于没有找到对应的MAC地址进而先发送的ARP request报文
 *        此时，收到了该request的应答报文。然后，根据IP地址来查找ARM表项，如果能找到该IP地址对应的MAC地址，
 *        则将缓存的数据包arp_buf再发送到ethernet层。
 * 
 *        如果arp_buf无效，还需要判断接收到的报文是否为request请求报文，并且，该请求报文的目的IP正好是本机的IP地址，
 *        则认为是请求本机MAC地址的ARP请求报文，则回应一个响应报文（应答报文）。
 *        响应报文：需要调用buf_init初始化一个buf，填写ARP报头，目的IP和目的MAC需要填写为收到的ARP报的源IP和源MAC。
 * 
 * @param buf 要处理的数据包
 */
void arp_in(buf_t *buf)
{
    //获取arp包
    arp_pkt_t *arp_pkt = (arp_pkt_t *)buf->data;
    int opcode = swap16(arp_pkt->opcode);
    //报头检查
    if(arp_pkt->hw_type != swap16(ARP_HW_ETHER) 
    || arp_pkt->pro_type != swap16(NET_PROTOCOL_IP) 
    || arp_pkt->hw_len != NET_MAC_LEN
    || arp_pkt->pro_len != NET_IP_LEN
    || (opcode != ARP_REQUEST && opcode != ARP_REPLY) ){
        return;
    }
    //根据发送者ip和mac更新arp表
    arp_update(arp_pkt->sender_ip,arp_pkt->sender_mac,ARP_VALID);
    //无效--即队列为空
    for (int i = 0; i < ARP_MAX_ENTRY; i++){
        //因为是按序存储，所以找到valid为0的buf即可break。
        if(arp_buf[i].valid == 0){
            //若为请求报文
            if(opcode == ARP_REQUEST && memcmp(arp_pkt->target_ip, net_if_ip, NET_IP_LEN) == 0 ){
                //向请求方发送应答报文
                arp_reply(arp_pkt->sender_mac,arp_pkt->sender_ip);
            }
            break;
        }
        else{
            uint8_t* mac = arp_lookup(arp_buf[i].ip);
            if(mac != NULL){
                arp_buf[i].valid = 0;
                ethernet_out(&arp_buf[i].buf,mac,arp_buf[i].protocol);
            }
        }
    }
}

/**
 * @brief 处理一个要发送的数据包
 *        你需要根据IP地址来查找ARP表
 *        如果能找到该IP地址对应的MAC地址，则将数据报直接发送给ethernet层
 *        如果没有找到对应的MAC地址，则需要先发一个ARP request报文。
 *        注意，需要将来自IP层的数据包缓存到arp_buf中，等待arp_in()能收到ARP request报文的应答报文
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO
    uint8_t* mac = arp_lookup(ip);
    if(mac != NULL){
        ethernet_out(buf,mac,protocol);
    }
    else{
        for (int i = 0; i < ARP_MAX_ENTRY; i++){
            if(arp_buf[i].valid == 0){
                buf_copy(&arp_buf[i].buf,buf);
                arp_buf[i].valid = 1;
                memcpy(arp_buf[i].ip,ip,NET_IP_LEN);
                arp_buf[i].protocol = protocol;
                break;
            }
        }
        arp_req(ip);
    }
}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init()
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_table[i].state = ARP_INVALID;
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_buf[MAX_BUF_NUMBER].valid = 0;
    arp_req(net_if_ip);
}