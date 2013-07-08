#ifndef __ICMP_H__
#define __ICMP_H__

enum icmp_type {
	ICMP_REQUEST,
	ICMP_REPLY,
};

struct icmp_packet {
	struct sockaddr_storage peer;
	int peer_len;
	enum icmp_type type;
	uint16_t id;
	uint16_t seqno;
	uint8_t *payload;
	uint32_t payload_len;
};

extern int icmp_parse(struct icmp_packet *pkt, uint8_t *data, int len);
extern void icmp_dump(struct icmp_packet *pkt);
extern void icmp_send(int socket, struct icmp_packet *pkt);

#endif

