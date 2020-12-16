#include "icmp.h"
#include "ip.h"
#include <string.h>
#include <stdio.h>

static int global_seq = 0;
/**
 * @brief 处理一个收到的数据包
 *        你首先要检查ICMP报头长度是否小于icmp头部长度
 *        接着，查看该报文的ICMP类型是否为回显请求，
 *        如果是，则回送一个回显应答（ping应答），需要自行封装应答包。
 * 
 *        应答包封装如下：
 *        首先调用buf_init()函数初始化txbuf，然后封装报头和数据，
 *        数据部分可以拷贝来自接收到的回显请求报文中的数据。
 *        最后将封装好的ICMP报文发送到IP层。  
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    // TODO
    icmp_hdr_t * icmp_hdr = (icmp_hdr_t *) buf->data;
    //检查ICMP报文长度是否小于ICMP头部长度
    if(buf->len < sizeof(ip_hdr_t)){
        return;
    }
    //判断是否为回显请求
    if(icmp_hdr->type == ICMP_TYPE_ECHO_REQUEST && icmp_hdr->code == 0){
        global_seq++;
        buf_init(&txbuf, buf->len);
        //将原数据报的数据拷贝进要发送的数据报中
        memcpy(txbuf.data + sizeof(icmp_hdr_t),buf->data + sizeof(icmp_hdr_t),buf->len); 
        icmp_hdr_t * icmp_hdr = (icmp_hdr_t *) txbuf.data;
        icmp_hdr->type = ICMP_TYPE_ECHO_REPLY;
        icmp_hdr->code = 00;
        icmp_hdr->checksum = swap16(00);
        icmp_hdr->id = swap16(global_seq);
        icmp_hdr->seq = swap16(global_seq);
        icmp_hdr->checksum = swap16(checksum16((uint16_t *) (txbuf.data), txbuf.len / 2));
        ip_out(&txbuf,src_ip,NET_PROTOCOL_ICMP);
    }
}

/**
 * @brief 发送icmp不可达
 *        你需要首先调用buf_init初始化buf，长度为ICMP头部 + IP头部 + 原始IP数据报中的前8字节 
 *        填写ICMP报头首部，类型值为目的不可达
 *        填写校验和
 *        将封装好的ICMP数据报发送到IP层。
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    // TODO
    //总长度为ICMP头部 + IP头部 + 原始IP数据报中的前8字节
    int total_length = sizeof(icmp_hdr_t) + sizeof(ip_hdr_t) + 8;
    buf_init(&txbuf, total_length);
    //拷贝原数据报的IP头及数据报中的前8字节
    memcpy(txbuf.data + sizeof(icmp_hdr_t),recv_buf->data,sizeof(ip_hdr_t) + 8); 
    //设置ICMP报头
    icmp_hdr_t * icmp_hdr = (icmp_hdr_t *) txbuf.data ;
    icmp_hdr->type = ICMP_TYPE_UNREACH;
    icmp_hdr->code = code;
    icmp_hdr->id = swap16(00);
    icmp_hdr->seq = swap16(00);
    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = swap16(checksum16((uint16_t *) txbuf.data  , total_length / 2));
    ip_out(&txbuf,src_ip,NET_PROTOCOL_ICMP);
}