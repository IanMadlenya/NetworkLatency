#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <inttypes.h>
#include <limits.h>
#include <argp.h>

#include <unistd.h>

#define RECEIVER_PORT 10000


static struct sockaddr_in sockaddr;

typedef struct payload_s {
	uint32_t data;
	uint32_t id;
} __attribute__ ((__packed__)) payload_t;



// Program documentation
static char doc[] = "NetworkLatency receiver -- a program to receive and display the output of the Maxeler NetworkLatency bitstream application";
static char args_doc[] = "";

// The options the program accepts
static struct argp_option options[] = {
		{"cpu_ip_bot",	'c',	"CPUIPBOT",	0,	"IP Address to listen to.", 0 },
		{"port_bot",	't',	"PORTBOT",	0,	"Port number to use for the bottom DFE network port.", 0 },

		{"csv",		'v',	0,		0,	"Output in CSV format instead of the default human-readable format.", 0 },
		{ 0, 0, 0, 0, 0, 0 }
};

//used by main() to communicate with parse_opt()
struct arguments {
	struct in_addr cpu_ip_bot;
	int port_bot;
	int csv;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch (key) {
		case 'c':
			inet_aton(arg, &arguments->cpu_ip_bot);
			break;
		case 't':
			arguments->port_bot = atoi(arg);
			break;
		case 'v':
			arguments->csv = 1;
			break;
		case ARGP_KEY_ARG:
			argp_usage(state);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

// argp parser
static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };


uint16_t get_consumer_port(size_t consumer_index)
{
	return RECEIVER_PORT + consumer_index;
}

static int create_socket(struct in_addr *local_ip, int port) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(get_consumer_port(0)); //htons(port);

	sockaddr.sin_addr = *local_ip;
	if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		printf("Bind failed: %s\n", strerror(errno));
		exit(1);
	}

	return sock;
}


static void recv_frames(int sock, int csv) {
	payload_t received_frame;

	while (true) {
		int now = recv(sock, &received_frame, sizeof(payload_t), 0);
		if (now < 0) {
			printf("recvfrom error: %s\n", strerror(errno));
			exit(1);
		}

		double latency = ((double) received_frame.data) / 1000000;

		if (csv) {
			printf("'R',%08" PRIu32 ",%g\n", received_frame.id, latency);
		} else {
			printf("Received ID #%08" PRIu32 " with %gs latency.\n", received_frame.id, latency);
		}
		fflush(stdout);
	}
}

int main(int argc, char *argv[]) {
	struct arguments arguments;
	//Defaults
	inet_aton("172.16.60.10", &arguments.cpu_ip_bot);
	arguments.port_bot = 5008;
	arguments.csv = 0;

	//Parse the arguments
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	int mySocket = create_socket(&arguments.cpu_ip_bot, arguments.port_bot);

	recv_frames(mySocket, arguments.csv);

	return 0;
}

