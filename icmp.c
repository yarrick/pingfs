#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

uint16_t checksum(uint8_t *data, uint32_t len)
{
	uint32_t csum = 0;
	uint32_t i;
	for (i = 0; i < len; i += 2) {
		uint16_t c = data[i] << 8;
		if (i + 1 < len) c |= data[i + 1];
		csum += c;
	}
	csum = (csum >> 16) + (csum & 0xffff);
	csum += (csum >> 16);
	return (uint16_t)(~csum);
}

// v4 socket will return full IP header
int open_raw_v4_socket()
{
	return socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
}

#if 0
// v6 socket will just give ICMPv6 data, no IP header
int open_raw_v6_socket()
{
	int sock = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sock >= 0) {
		struct icmp6_filter filter;
		ICMP6_FILTER_SETBLOCKALL(&filter);
		ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
		setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter));
	}
	return sock;
}
#endif

int main(void)
{
	uint8_t buf[1024];
	int fd = open_raw_v4_socket();
	if (fd<0) {
		perror("rawsock");
		exit(1);
	}
	int i = recv(fd, buf, 1024, 0);
	printf("got result %d\n", i);
	if (i > 0) {
		int d;
		for (d = 0; d < i; d++) {
			printf("%02X ", buf[d]);
		}
		printf("\n");
	}
}



