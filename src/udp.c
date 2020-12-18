#include "udp.h"
#include "ip.h"
#include "icmp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/**
 * @brief udp处理程序表
 * 
 */
static udp_entry_t udp_table[UDP_MAX_HANDLER];

void test_handler(udp_entry_t *entry, uint8_t *src_ip, uint16_t src_port, buf_t *buf)
{
    printf("recv udp packet from %s:%d len=%d\n", iptos(src_ip), src_port, buf->len);
    for (int i = 0; i < buf->len ; i++)
        putchar(buf->data[i]);
    putchar('\n');
    uint16_t len = 1800;
    // uint16_t len = 1000;
    uint8_t data[len];

    uint16_t dest_port = 60001;
    for (int i = 0; i < len; i++)
        data[i] = i;
    udp_send(data, len, 60000, src_ip, dest_port); //发送udp包
}


/**
 * @brief udp伪校验和计算
 *        1. 你首先调用buf_add_header()添加UDP伪头部
 *        2. 将IP头部拷贝出来，暂存被UDP伪头部覆盖的IP头部
 *        3. 填写UDP伪头部的12字节字段
 *        4. 计算UDP校验和，注意：UDP校验和覆盖了UDP头部、UDP数据和UDP伪头部
 *        5. 再将暂存的IP头部拷贝回来
 *        6. 调用buf_remove_header()函数去掉UDP伪头部
 *        7. 返回计算后的校验和。  
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dest_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dest_ip)
{
    // TODO
    //UDP头
    udp_hdr_t * udp_hdr = (udp_hdr_t *)buf->data;
    buf_add_header(buf,sizeof(udp_peso_hdr_t));
    //伪头部
    udp_peso_hdr_t * udp_peso_hdr = (udp_peso_hdr_t *)buf->data;
    memcpy(udp_peso_hdr->src_ip,src_ip,NET_IP_LEN); 
    memcpy(udp_peso_hdr->dest_ip,dest_ip,NET_IP_LEN); 
    udp_peso_hdr->placeholder = 0;
    udp_peso_hdr->protocol = NET_PROTOCOL_UDP;
    udp_peso_hdr->total_len = udp_hdr->total_len;
    //长度为buf->len除2
    int len = buf->len / 2;
    //将buf->data使用16位处理
    uint16_t *data = (uint16_t *)buf->data;
    int sum = 0;
    for(int i = 0 ; i < len; i++){
        sum += swap16(data[i]);
    }
    //若长度为奇数
    if(buf->len % 2 != 0){
        sum += swap16(buf->data[len] << 8);
    }
    //进位加到低16位
    sum = (sum & 0xffff) + (sum >> 16);
    //加上最后的进位
    sum += (sum >> 16);
    //取反
    sum = ~( sum | 0xffff0000);
    buf_remove_header(buf,sizeof(udp_peso_hdr_t));
    return sum;
}

/**
 * @brief 处理一个收到的udp数据包
 *        你首先需要检查UDP报头长度
 *        接着计算checksum，步骤如下：
 *          （1）先将UDP首部的checksum缓存起来
 *          （2）再将UDP首都的checksum字段清零
 *          （3）调用udp_checksum()计算UDP校验和
 *          （4）比较计算后的校验和与之前缓存的checksum进行比较，如不相等，则不处理该数据报。
 *       然后，根据该数据报目的端口号查找udp_table，查看是否有对应的处理函数（回调函数）
 *       
 *       如果没有找到，则调用buf_add_header()函数增加IP数据报头部(想一想，此处为什么要增加IP头部？？)
 *       然后调用icmp_unreachable()函数发送一个端口不可达的ICMP差错报文。
 * 
 *       如果能找到，则去掉UDP报头，调用处理函数（回调函数）来做相应处理。
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{ 
    // TODO    
    //检查UDP报头长度
    if(buf->len < sizeof(udp_hdr_t)){
        return;
    }
    udp_hdr_t * udp_hdr = (udp_hdr_t *)buf->data;
    uint16_t src_port = swap16(udp_hdr->src_port);          //源端口
    uint16_t dest_port = swap16(udp_hdr->dest_port);        //目的端口
    //buf长度设为真实长度
    buf->len = swap16(udp_hdr->total_len);
    //计算checksum
    uint16_t checksum = swap16(udp_hdr->checksum);
    udp_hdr->checksum = 0;
    if(checksum != udp_checksum(buf,src_ip,net_if_ip)){
        return;
    }
    //还原
    udp_hdr->checksum = swap16(checksum);
    //没有找到
    if(udp_open(dest_port,test_handler) == -1){
        //增加IP数据报头部
        buf_add_header(buf,sizeof(ip_hdr_t));
        ip_hdr_t *ip_hdr = (ip_hdr_t *) buf->data;
        ip_hdr->version = IP_VERSION_4;                                         //协议版本
        ip_hdr->hdr_len = 5;                                                    //首部长度
        ip_hdr->tos = 0;                                                        //区分服务
        ip_hdr->total_len = swap16(buf->len);                                   //总长度
        ip_hdr->id = swap16(0);                                                 //标识
        ip_hdr->flags_fragment = swap16(0);                                     //标志和片偏移
        ip_hdr->ttl = IP_DEFALUT_TTL;                                           //生存时间
        ip_hdr->protocol = NET_PROTOCOL_UDP;                                    //协议
        memcpy(ip_hdr->src_ip,net_if_ip,NET_IP_LEN);                            //源地址
        memcpy(ip_hdr->dest_ip,src_ip,NET_IP_LEN);                              //目的地址
        ip_hdr->hdr_checksum = 0;
        ip_hdr->hdr_checksum = swap16(checksum16((uint16_t *)buf->data,10));    //首部检验和
        //发送端口不可达的ICMP差错报文
        icmp_unreachable(buf,src_ip,ICMP_CODE_PORT_UNREACH);
    }
    //如果找到
    else{
        buf_remove_header(buf,sizeof(udp_hdr_t));
        test_handler(udp_table,src_ip,src_port,buf);
    }
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要调用buf_add_header()函数增加UDP头部长度空间
 *        填充UDP首部字段
 *        调用udp_checksum()函数计算UDP校验和
 *        将封装的UDP数据报发送到IP层。    
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    // TODO
    buf_add_header(buf,sizeof(udp_hdr_t));
    udp_hdr_t * udp_hdr = (udp_hdr_t *)buf->data;
    udp_hdr->src_port = swap16(src_port);
    udp_hdr->dest_port = swap16(dest_port);
    udp_hdr->total_len = swap16(buf->len);
    udp_hdr->checksum = 0;
    uint16_t checksum = udp_checksum(buf,net_if_ip,dest_ip);
    udp_hdr->checksum = swap16(checksum);
    ip_out(buf,dest_ip,NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init()
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        udp_table[i].valid = 0;
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图更新
        if (udp_table[i].port == port)
        {
            udp_table[i].handler = handler;
            udp_table[i].valid = 1;
            return 0;
        }

    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图插入
        if (udp_table[i].valid == 0)
        {
            udp_table[i].handler = handler;
            udp_table[i].port = port;
            udp_table[i].valid = 1;
            return 0;
        }
    return -1;
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        if (udp_table[i].port == port)
            udp_table[i].valid = 0;
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dest_ip, dest_port);
}