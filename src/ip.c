#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include <string.h>

#define MAX_LIVE_TIME 64
static int global_id = 0;
/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，检查项包括：版本号、总长度、首部长度等。
 * 
 *        接着，计算头部校验和，注意：需要先把头部校验和字段缓存起来，再将校验和字段清零，
 *        调用checksum16()函数计算头部检验和，比较计算的结果与之前缓存的校验和是否一致，
 *        如果不一致，则不处理该数据报。
 * 
 *        检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报。
 * 
 *        检查IP报头的协议字段：
 *        如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
 *        如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
 *        如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
 *          
 * @param buf 要处理的包
 */
void ip_in(buf_t *buf)
{
    // TODO 
    ip_hdr_t *ip_hdr = (ip_hdr_t *) buf->data;
    uint16_t total_len = swap16(ip_hdr->total_len);
    uint16_t id = swap16(ip_hdr->id);
    uint16_t flags_fragment = swap16(ip_hdr->flags_fragment);
    uint8_t hdr_len = ip_hdr->hdr_len * IP_HDR_LEN_PER_BYTE;
    uint8_t src_ip[NET_IP_LEN];
    memcpy(src_ip,ip_hdr->src_ip,NET_IP_LEN);
    //报头检查
    if(ip_hdr->version != IP_VERSION_4
    || (hdr_len < 20 || hdr_len > 60)
    || total_len > 65535){
        return;
    }
    //缓存校验和
    uint16_t checksum = swap16(ip_hdr->hdr_checksum);
    ip_hdr->hdr_checksum = 0;
    //总长度大于分片长度且DF分片位为1
    if(total_len - hdr_len > ETHERNET_MTU && (flags_fragment & 1) == 1){
        return;
    }
    //若自带的检验和同计算出的检验和不同则丢弃
    if(checksum != checksum16((uint16_t *)buf->data , hdr_len / 2 ) ){
        return;
    }
    //若目的IP不为本机IP则不处理
    if( memcmp(ip_hdr->dest_ip, net_if_ip, NET_IP_LEN) != 0){
        return;
    }
    switch(ip_hdr->protocol){
        case NET_PROTOCOL_UDP:
            buf_remove_header(buf,hdr_len);
            udp_in(buf,src_ip);
            break;
        case NET_PROTOCOL_ICMP:
            buf_remove_header(buf,hdr_len);
            icmp_in(buf,src_ip);
            break;
        default:
            ip_hdr->hdr_checksum = swap16(checksum);
            icmp_unreachable(buf,src_ip,ICMP_CODE_PROTOCOL_UNREACH);
            break;
    }
}

/**
 * @brief 处理一个要发送的ip分片
 *        你需要调用buf_add_header增加IP数据报头部缓存空间。
 *        填写IP数据报头部字段。
 *        将checksum字段填0，再调用checksum16()函数计算校验和，并将计算后的结果填写到checksum字段中。
 *        将封装后的IP数据报发送到arp层。
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TODO
    buf_add_header(buf,20);
    ip_hdr_t *ip_hdr = (ip_hdr_t *) buf->data;
    ip_hdr->version = IP_VERSION_4;                                         //协议版本
    ip_hdr->hdr_len = 5;                                                    //首部长度
    ip_hdr->tos = 0;                                                        //区分服务
    //若没有分片则总长度即为buf长度
    if(mf == 0){
        ip_hdr->total_len = swap16(buf->len);                               //总长度
    }
    else{
        ip_hdr->total_len = swap16(ETHERNET_MTU);                           //总长度  
    }
    ip_hdr->id = swap16(id);                                                //标识
    ip_hdr->flags_fragment = swap16(offset >> 3 | mf << 13);                //标志和片偏移
    ip_hdr->ttl = MAX_LIVE_TIME;                                            //生存时间
    ip_hdr->protocol = protocol;                                            //协议
    memcpy(ip_hdr->src_ip,net_if_ip,NET_IP_LEN);                            //源地址
    memcpy(ip_hdr->dest_ip,ip,NET_IP_LEN);                                  //目的地址
    ip_hdr->hdr_checksum = 0;
    ip_hdr->hdr_checksum = swap16(checksum16((uint16_t *)buf->data,10));    //首部检验和
    arp_out(buf,ip,NET_PROTOCOL_IP);
}

/**
 * @brief 处理一个要发送的ip数据包
 *        你首先需要检查需要发送的IP数据报是否大于以太网帧的最大包长（1500字节 - 以太网报头长度）。
 *        
 *        如果超过，则需要分片发送。 
 *        分片步骤：
 *        （1）调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - 以太网报头长度）
 *        （2）将数据报截断，每个截断后的包长度 = 以太网帧的最大包长，调用ip_fragment_out()函数发送出去
 *        （3）如果截断后最后的一个分片小于或等于以太网帧的最大包长，
 *             调用buf_init()函数初始化buf，长度为该分片大小，再调用ip_fragment_out()函数发送出去
 *             注意：id为IP数据报的分片标识，从0开始编号，每增加一个分片，自加1。最后一个分片的MF = 0
 *    
 *        如果没有超过以太网帧的最大包长，则直接调用调用ip_fragment_out()函数发送出去。
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO 
    int id = global_id;
    uint16_t offset = 0;
    while(buf->len - 20 > ETHERNET_MTU){
        buf_init(&txbuf,ETHERNET_MTU - 20);
        memcpy(txbuf.data,buf->data,buf->len);
        ip_fragment_out(&txbuf,ip,protocol,id,offset,1);
        buf_remove_header(buf,ETHERNET_MTU - 20);
        offset += ETHERNET_MTU - 20;
    }
    buf_init(&txbuf,buf->len);
    memcpy(txbuf.data,buf->data,buf->len);
    ip_fragment_out(&txbuf,ip,protocol,id,offset,0);
    global_id++;
}