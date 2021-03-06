
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <limits.h>
#include <errno.h>

#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#define AGING_TIME 30 /* 30 seconds */
#define MAX_SWITCH_PORTS 8
#define MAX_PACKET_SIZE 65536

struct mac_entry
{
        struct ether_addr addr;
        struct eth_switch_port *output_port;
        struct avl_node avl_node;
        struct timeval expiration_time;
};

struct eth_switch_port
{
        struct list_head lentry;
        int ifindex;
        int sock;
};

struct eth_switch
{
        struct eth_switch_port ports[MAX_SWITCH_PORTS];
        int nports;
        struct avl_tree mac_table;
};

struct eth_frame
{
        uint8_t buf[MAX_PACKET_SIZE];
        size_t len;
        struct ether_addr source;
        struct ether_addr dest;

        struct eth_switch_port *input_port;
};

int avl_maccmp(const void *k1, const void *k2, void *ptr)
{
        const struct ether_addr *mac1 = k1, *mac2 = k2;
        int64_t mac1_int = 0, mac2_int = 0;

        memcpy(&mac1_int, mac1, sizeof(*mac1));
        memcpy(&mac2_int, mac2, sizeof(*mac2));

        if (mac1_int - mac2_int > 0)
                return 1;
        else if (mac1_int - mac2_int < 0)
                return -1;
        else
                return 0;
}

static bool frame_is_broadcast(struct eth_frame *frame)
{
        uint8_t *dest = frame->dest.ether_addr_octet;

        if (dest[0] == 0xff &&
            dest[1] == 0xff &&
            dest[2] == 0xff &&
            dest[3] == 0xff &&
            dest[4] == 0xff &&
            dest[5] == 0xff)
                return true;
        else
                return false;
}

static void dump_mac_table(struct eth_switch *sw)
{
        struct mac_entry *entry;
        struct timeval now, diff = {0};

        gettimeofday(&now, NULL);

        avl_for_each_element(&sw->mac_table, entry, avl_node)
        {
                if (timercmp(&now, &entry->expiration_time, <))
                        timersub(&now, &entry->expiration_time, &diff);

                printf("%s , interface %d, expires in %d seconds\n",
                       ether_ntoa(&entry->addr),
                       entry->output_port->ifindex, (int)diff.tv_sec);
        }
}

/* Функция задает значение timeout - разницу между текущим временем и самым
 * ранним временем устаревания для всех MAC-адресов. Используется функция
 * gettimeofday() и макросы timeradd(), timersub(), timercmp() и
 * avl_for_each_element().
 */
static void set_aging_timeout(struct eth_switch *sw, struct timeval *timeout)
{

        /* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */
}

/* Функция проверяет все записи в таблице MAC-адресов и удаляет устаревшие
 * записи. Используются функции и макросы avl_for_each_element_safe(),
 * avl_delete(), timercmp().
 */
static void handle_aging_timeout(struct eth_switch *sw)
{

        /* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */
}

/* Функция проверяет наличие адреса источника Ethernet-кадра в таблице
 * MAC-адресов. Если MAC-адрес присутствует в таблице, программа обновляет
 * время устаревания этой записи. Если адрес отсутствует, программа создает
 * новую запись в таблице. Используются функции avl_find_element(),
 * avl_insert().
 */
static int learn_mac_address(struct eth_switch *sw, struct eth_frame *frame)
{

        /* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */

        return 0;
}

/* Функция проверяет наличие адреса назначения Ethernet-кадра в таблице
 * MAC-адресов. Если адрес присутствует в таблице, программа удаляет из маски
 * выходных портов, кроме порта, указанного в записи таблицы.
 */
static int filter_output_ports(struct eth_switch *sw,
                               struct eth_frame *frame,
                               struct list_head *output_ports)
{


        return 0;
}

/* Функция записывает Ethernet-кадр @frame в сокет AF_PACKET порта @port. */
static int tx_frame(struct eth_switch_port *port, struct eth_frame *frame)
{

        fprintf(stderr,"-159- port - %d, ifindex - %d\n",port->sock, port->ifindex);
        int ret = 0;
        int sock = port->sock;

        printf("Sending frame to interface %d\n", port->ifindex);
        ret = send(sock, &frame->buf, frame->len,0);

        if (ret < 0)
        {
                fprintf(stderr,
                        "Failed to send frame to interface %d: %s\n",
                        port->ifindex, strerror(errno));
                return -1;
        }

        return 0;
}

/* Функция читает кадр из сокета AF_PACKET, заполняет объект frame (номер
 * входного порта, адрес источника и назначения). Функция проверяет тип кадра.
 * Если это тип PACKET_OUTGOING, кадр отбрасывается, и функция возвращает 0. В
 * противном случае кадр считается принятым, и функция возвращает 1.
 */
static int rx_frame(struct eth_switch_port *port, struct eth_frame *frame)
{
        fprintf(stderr,"Recieving frame\n");
        int ret = 0;
        int sock = port->sock;
        struct sockaddr_ll rx_addr;
        socklen_t addrlen = sizeof(rx_addr);
        struct ethhdr *ethhdr;

        ret = recvfrom(sock,
                       &frame->buf,
                       MAX_PACKET_SIZE,
                       0,
                       (struct sockaddr *)&rx_addr,
                       &addrlen);
        if (ret < 0)
        {
                fprintf(stderr,
                        "Failed to receive frame on interface %d: %s\n",
                        port->ifindex, strerror(errno));
                return -1;
        }

        if (rx_addr.sll_pkttype == PACKET_OUTGOING)
                return 0;
        else
        {
                frame->len = ret;
                ethhdr = frame->buf;
                memcpy(&frame->source, ethhdr->h_source, ETH_ALEN);
                memcpy(&frame->dest, ethhdr->h_dest, ETH_ALEN);
                frame->input_port = port;
                char sa_str[20], da_str[20];
                printf("Received frame: SA %s DA %s on port %d\n",
                       ether_ntoa_r(&frame->source, sa_str),
                       ether_ntoa_r(&frame->dest, da_str),
                       port->ifindex);
        }

        return 1;
}

static int handle_input_frame(struct eth_switch *sw, struct eth_switch_port *input_port)
{
        struct eth_frame frame;
        struct list_head output_ports;
        struct eth_switch_port *port;
        int ret;
        int i;

        ret = rx_frame(input_port, &frame);
        if (ret < 0)
                goto on_error;
        else if (ret == 0)
                return 0;

        //ret = learn_mac_address(sw, &frame);
        if (ret < 0)
                goto on_error;

        INIT_LIST_HEAD(&output_ports);
        for (i = 0; i < sw->nports; i++)
                if (&sw->ports[i] != input_port)
                        list_add(&sw->ports[i].lentry, &output_ports);

        if (!frame_is_broadcast(&frame))
        {
                //ret = filter_output_ports(sw, &frame, &output_ports);
                if (ret < 0)
                        goto on_error;
        }

        list_for_each_entry(port, &output_ports, lentry)
        {
                fprintf(stderr,"Sending\n");
                ret = tx_frame(port, &frame);
                if (ret < 0)
                        goto on_error;
        }

        return 0;
on_error:
        return -1;
}

/* Функция запоминает в поле порта индекс интерфейса @ifindex. Далее функция
 * создает сокет типа AF_PACKET и привязывает его к интерфейсу с индексом
 * @ifindex. Используются функции socket() и bind().
 */
static int init_switch_port(struct eth_switch_port *port, int ifindex)
{

        int sock = -1;
        struct sockaddr_ll addr;
        int ret = 0;

        sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (sock < 0)
        {
                fprintf(stderr, "Failed to create packet socket: %s\n", strerror(errno));
                return -1;
        }

        memset(&addr, 0, sizeof(struct sockaddr_ll));
        addr.sll_family = AF_PACKET;
        addr.sll_ifindex = ifindex;

        ret = bind(sock, (const struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1)
        {
                fprintf(stderr, "Failed to bind socket to ifindex %d: %s\n",
                        ifindex, strerror(errno));
                return -1;
        }

        port->ifindex = ifindex;
        port->sock = sock;

        return 0;
}

static void cleanup_switch(struct eth_switch *sw)
{
        int i;

        for (i = 0; i < sw->nports; i++)
                close(sw->ports[i].sock);
}

static int init_switch(struct eth_switch *sw, char *indices[])
{
        int i;
        int ret;

        memset(sw, 0, sizeof(*sw));

        avl_init(&sw->mac_table, avl_maccmp, false, NULL);
        for (i = 0; i < MAX_SWITCH_PORTS && indices[i]; i++)
        {
                int ifindex = atoi(indices[i]);
                ret = init_switch_port(&sw->ports[i], ifindex);
                if (ret < 0)
                        goto on_error;
                sw->nports++;
        }
        return 0;
on_error:
        cleanup_switch(sw);
        return -1;
}

int main(int argc, char *argv[])
{
        struct eth_switch _sw, *sw = &_sw;
        int ret;
        int i;

        if (argc == 1)
        {
                printf("Specify indices of switch ports\n");
                return 0;
        }

        ret = init_switch(sw, argv + 1);
        if (ret < 0)
                return 1;

        while (1)
        {
                fd_set fds;
                int max_fd = 0;
                struct timeval timeout = {.tv_sec = LONG_MAX};

                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                max_fd = STDIN_FILENO;

                for (i = 0; i < sw->nports; i++)
                {
                        FD_SET(sw->ports[i].sock, &fds);
                        if (sw->ports[i].sock > max_fd)
                                max_fd = sw->ports[i].sock;
                }

                //set_aging_timeout(sw, &timeout);

                ret = select(max_fd + 1, &fds, NULL, NULL, &timeout);
                if (ret < 0)
                {
                        fprintf(stderr, "select() failed: %s\n", strerror(errno));
                        goto on_error;
                }

                if (ret == 0)
                        //handle_aging_timeout(sw);

                if (FD_ISSET(STDIN_FILENO, &fds))
                {
                        char buf[1024];

                        read(STDIN_FILENO, buf, sizeof(buf));
                        dump_mac_table(sw);
                }

                for (i = 0; i < sw->nports; i++)
                {
                        if (FD_ISSET(sw->ports[i].sock, &fds))
                        {
                                ret = handle_input_frame(sw, &sw->ports[i]);
                                if (ret < 0)
                                        goto on_error;

                        }
                }
        }

        cleanup_switch(sw);
        return 0;
on_error:
        cleanup_switch(sw);
        return 1;
}
