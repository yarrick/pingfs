#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>

#include "icmp.h"

static int sockv4;
static int sockv6;


static void ping_host(uint8_t *buf)
{
	static int seqno = 0x45;
	char hostname[256];
	struct icmp_packet pkt;
	struct addrinfo *result;
	int sock = -1;
	int error;

	bzero(hostname, sizeof(hostname));
	sscanf(buf, "%255s", hostname);

	if (!strlen(hostname)) return;

	printf("Resolving %s...", hostname);
	fflush(stdout);
	error = getaddrinfo(hostname, NULL, NULL, &result);
	if (error) {
		if (error == EAI_SYSTEM) {
			perror("getaddrinfo");
		} else {
			printf(" failed: %s\n", gai_strerror(error));
		}
		return;
	}

	memcpy(&pkt.peer, result->ai_addr, result->ai_addrlen);
	pkt.peer_len = result->ai_addrlen;
	pkt.type = ICMP_REQUEST;
	pkt.id = 0xFEF0;
	pkt.seqno = seqno++;
	pkt.payload = hostname;
	pkt.payload_len = strlen(hostname);

	if (ICMP_ADDRFAMILY(&pkt) == AF_INET) {
		sock = sockv4;
	} else {
		sock = sockv6;
	}

	printf("\nSending ");
	icmp_dump(&pkt);
	icmp_send(sock, &pkt);

	freeaddrinfo(result);
}

static void print_prompt()
{
	printf("> ");
	fflush(stdout);
}

static void read_reply(int sock)
{
	struct icmp_packet mypkt;
	mypkt.peer_len = sizeof(struct sockaddr_storage);
	uint8_t buf[BUFSIZ];
	int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &mypkt.peer, &mypkt.peer_len);
	if (len > 0) {
		if (icmp_parse(&mypkt, buf, len) == 0) {
			printf("\rReceived ");
			icmp_dump(&mypkt);
			free(mypkt.payload);
			print_prompt();
		}
	}
}

int main(void)
{
	sockv4 = open_icmpv4_socket();
	if (sockv4<0) {
		perror("Failed to open IPv4 socket");
		exit(1);
	}
	sockv6 = open_icmpv6_socket();
	if (sockv6<0) {
		perror("IPv6 not enabled");
	}

	printf("Type a hostname to ping it (^C to quit)\n");
	print_prompt();

	int maxfd = sockv4;
	if (sockv6>sockv4) maxfd=sockv6;
	for (;;) {
		struct timeval tv;
		fd_set fds;
		int i;

		FD_ZERO(&fds);
		FD_SET(sockv4, &fds);
		if (sockv6 > 0) FD_SET(sockv6, &fds);
		FD_SET(STDIN_FILENO, &fds);

		tv.tv_sec = 10;
		tv.tv_usec = 0;
		i = select(maxfd+1, &fds, NULL, NULL, &tv);
		if (FD_ISSET(sockv4, &fds)) read_reply(sockv4);
		if ((sockv6 > 0) && FD_ISSET(sockv6, &fds)) read_reply(sockv6);
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			uint8_t buf[BUFSIZ];
			int k;
			bzero(buf, sizeof(buf));
			int len = read(STDIN_FILENO, buf, sizeof(buf) - 1);
			if (!len) continue;
			ping_host(buf);

			print_prompt();
		}
	}

	return 0;
}

