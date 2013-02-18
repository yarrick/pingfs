#ifndef __ICMP_H__
#define __ICMP_H__

struct icmp_packet {
	struct sockaddr_storage peer;
	int peer_len;
	uint8_t type;
	uint8_t code;
	uint16_t id;
	uint16_t seqno;
	uint8_t *payload;
	uint32_t payload_len;
};

#define ICMP_REPLY_TYPE 0
#define ICMP_REQUEST_TYPE 8

#define ICMP_IS_REPLY(x) ((x)->type == ICMP_REPLY_TYPE)
#define ICMP_IS_REQUEST(x) ((x)->type == ICMP_REQUEST_TYPE)

#define ICMP_MIN_LENGTH 8

extern int icmp_parse(struct icmp_packet *pkt, uint8_t *data, int len);
extern void icmp_dump(struct icmp_packet *pkt);
extern void icmp_send(int socket, struct icmp_packet *pkt);

#endif

