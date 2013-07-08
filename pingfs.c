#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>

#include "icmp.h"

static int sockv4;
static int sockv6;

static void send_icmp(struct icmp_packet *pkt)
{
	int sock = -1;
	if (ICMP_ADDRFAMILY(pkt) == AF_INET) {
		sock = sockv4;
	} else {
		sock = sockv6;
	}
	icmp_send(sock, pkt);
}

static read_reply(int sock)
{
	struct icmp_packet mypkt;
	mypkt.peer_len = sizeof(struct sockaddr_storage);
	uint8_t buf[BUFSIZ];
	printf("recv from sock %d\n", sock);
	int i = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &mypkt.peer, &mypkt.peer_len);
	if (i > 0) {
		int d = icmp_parse(&mypkt, buf, i);
		printf("len %d, parse res=%d\n", i, d);
		icmp_dump(&mypkt);
		free(mypkt.payload);
	}
	printf("res %d\n", i);
}

int main(void)
{
	uint8_t buf[2048];
	struct icmp_packet pkt;
	struct sockaddr_in *sockaddr = (struct sockaddr_in *) &pkt.peer;
	struct sockaddr_in6 *sockaddr6 = (struct sockaddr_in6 *) &pkt.peer;
	sockv4 = open_icmpv4_socket();
	if (sockv4<0) {
		perror("Failed to open IPv4 socket");
		exit(1);
	}
	sockv6 = open_icmpv6_socket();
	if (sockv6<0) {
		perror("Failed to open IPv6 socket");
		exit(1);
	}
	printf("v4 sock at %d, v6 at %d\n", sockv4, sockv6);

	// send
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = 0;
	sockaddr->sin_addr.s_addr = inet_addr("173.194.32.2"); //google.com
	pkt.peer_len = sizeof(struct sockaddr_in);
	pkt.type = ICMP_REQUEST;
	pkt.id = 0xFAFE;
	pkt.seqno = 123;
	pkt.payload = strdup("Foo123");
	pkt.payload_len = 7;

	send_icmp(&pkt);
	free(pkt.payload);

	// send v6
	sockaddr6->sin6_family = AF_INET6;
	sockaddr6->sin6_port = 0;
	inet_pton(AF_INET6, "2a00:1450:400f:800::1001", &sockaddr6->sin6_addr); // google.com
	pkt.peer_len = sizeof(struct sockaddr_in6);
	pkt.type = ICMP_REQUEST;
	pkt.id = 0xFAFE;
	pkt.seqno = 123;
	pkt.payload = strdup("Foo123");
	pkt.payload_len = 7;

	send_icmp(&pkt);
	free(pkt.payload);

	int maxfd = sockv4;
	if (sockv6>sockv4) maxfd=sockv6;
	for (;;) {
		struct timeval tv;
		fd_set fds;
		int i;

		FD_ZERO(&fds);
		FD_SET(sockv4, &fds);
		FD_SET(sockv6, &fds);

		tv.tv_sec = 10;
		tv.tv_usec = 0;
		i = select(maxfd+1, &fds, NULL, NULL, &tv);
		if (FD_ISSET(sockv4, &fds)) read_reply(sockv4);
		if (FD_ISSET(sockv6, &fds)) read_reply(sockv6);
	}

	return 0;
}

