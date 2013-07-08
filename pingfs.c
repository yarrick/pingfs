#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

static const struct addrinfo addr_request = {
	.ai_family = AF_UNSPEC,
	.ai_socktype = SOCK_RAW,
};

static int read_hosts(char *hostfile, struct gaicb **list[])
{
	char hostname[300];
	int h = 0;
	int listsize = 32;
	FILE *file;
	int i;

	struct gaicb *gais;

	file = fopen(hostfile, "r");
	if (!file) {
		perror("Failed to read file");
		return h;
	}

	gais = malloc(listsize * sizeof(struct gaicb));
	for (;;) {
		int res;
		memset(hostname, 0, sizeof(hostname));
		res = fscanf(file, "%256s", hostname);
		if (res == EOF) break;
		if (h == listsize) {
			listsize *= 2;
			gais = realloc(gais, listsize * sizeof(struct gaicb));
		}
		memset(&gais[h], 0, sizeof(struct gaicb));
		gais[h].ar_name = strndup(hostname, strlen(hostname));
		gais[h].ar_request = &addr_request;
		h++;
	}
	fclose(file);

	*list = calloc(h, sizeof(struct gaicb*));
	for (i = 0; i < h; i++) {
		(*list)[i] = &gais[i];
	}
	return h;
}

int main(int argc, char **argv)
{
	struct gaicb **list;
	int hosts;
	int i;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Expected one argument: path of hostname file\n");
		return EXIT_FAILURE;
	}

	hosts = read_hosts(argv[1], &list);
	if (!hosts) {
		fprintf(stderr, "No hosts configured! Exiting\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "Resolving %d hosts... ", hosts);
	fflush(stderr);

	ret = getaddrinfo_a(GAI_WAIT, list, hosts, NULL);
	if (ret != 0) {
		fprintf(stderr, "Resolving failed: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "done.\n");

	for (i = 0; i < hosts; i++) {
		ret = gai_error(list[i]);
		if (ret == 0) {
			struct addrinfo *result = list[i]->ar_result;
			char host[NI_MAXHOST];
			do {
				ret = getnameinfo(result->ai_addr, result->ai_addrlen,
						host, sizeof(host),
						NULL, 0, NI_NUMERICHOST);
				if (ret != 0) {
					fprintf(stderr, "getnameinfo() failed: %s\n",
							gai_strerror(ret));
					exit(EXIT_FAILURE);
				}
				printf("%s: %s\n", list[i]->ar_name, host);
				result = result->ai_next;
			} while (result);
		} else {
			fprintf(stderr, "Failed to resolve %s, skipping. (%s)\n", list[i]->ar_name, gai_strerror(ret));
		}
	}
	return EXIT_SUCCESS;
}
