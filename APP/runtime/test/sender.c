#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
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


// Program documentation
static char doc[] = "NetworkLatency sender -- a program to simulate sending packets from two network locations to the Maxeler NetworkLatency bitstream application";
static char args_doc[] = "";

// The options the program accepts
static struct argp_option options[] = {
		{"dfe_ip_top",	'D',	"DFEIPTOP",	0,	"IP Address for the top DFE network port.", 0 },
		{"port_top",	'T',	"PORTTOP",	0,	"Port number to use for the top DFE network port.", 0 },

		{"dfe_ip_bot",	'd',	"DFEIPBOT",	0,	"IP Address for the bottom DFE network port.", 0 },
		{"port_bot",	't',	"PORTBOT",	0,	"Port number to use for the bottom DFE network port.", 0 },

		{"latency",	'l',	"LATENCY",	0,	"Microseconds to add in between packets sent to top and bottom DFE ports, "
												"to simulate extra latency.", 0 },
		{"wait",	'w',	"WAIT",		0,	"Microseconds to wait in between each pair of packets sent. This will not "
												"add latency to the results, but it will create a pause between each pair of "
												"timestamps sent to the DFE.", 0},
		{"csv",		'v',	0,			0,	"Output in CSV format instead of the default human-readable format.", 0 },
		{"send",		's',	"SEND",		0,	"Send SEND total packets to each DFE port.", 0},
		{ 0, 0, 0, 0, 0, 0 }
};

//used by main() to communicate with parse_opt()
struct arguments {
	int latency, wait, csv;
	struct in_addr dfe_ip_top;
	int port_top;
	struct in_addr dfe_ip_bot;
	int port_bot;
	uint32_t send;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch (key) {
		case 'D':
			inet_aton(arg, &arguments->dfe_ip_top);
			break;
		case 'T':
			arguments->port_top = atoi(arg);
			break;
		case 'd':
			inet_aton(arg, &arguments->dfe_ip_bot);
			break;
		case 't':
			arguments->port_bot = atoi(arg);
			break;
		case 'l':
			arguments->latency = atoi(arg);
			break;
		case 'w':
			arguments->wait = atoi(arg);
			break;
		case 's':
			arguments->send = (uint32_t) atoll(arg);
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



typedef struct payload_s {
	uint32_t data;
	uint32_t id;
} __attribute__ ((__packed__)) payload_t;


static int create_socket(struct in_addr *remote_ip, int port) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);

	sockaddr.sin_addr = *remote_ip;
    connect(sock, (const struct sockaddr*) &sockaddr, sizeof(sockaddr));

	return sock;
}


static void send_frames(int sock_top, int sock_bot, struct arguments *args) {

	for (uint32_t j = 0; j < args->send; j++) {
		payload_t source_frame;

		source_frame.data = j+6;// rand();
		source_frame.id = j+1;

		send(sock_top, &source_frame, sizeof(payload_t), 0);

		usleep(args->latency);

		send(sock_bot, &source_frame, sizeof(payload_t), 0);

		usleep(args->wait);


		if (args->csv) {
			printf("'S',%08" PRIu32 ",0x%" PRIx32 ",%g,%g\n",
					source_frame.id,
					source_frame.data,
					((double) args->latency) / 1000000,
					((double) args->wait) / 1000000);
		} else {
			printf("Sent in ID #%08" PRIu32 " (data 0x%" PRIx32 ") with %gs additional latency and %gs pause between pairs.\n",
					source_frame.id,
					source_frame.data,
					((double) args->latency) / 1000000,
					((double) args->wait) / 1000000);
		}
		fflush(stdout);
	}
}


int main(int argc, char *argv[]) {
	struct arguments arguments;
	//Defaults
	inet_aton("172.16.50.1", &arguments.dfe_ip_top);
	arguments.port_top = 5007;

	inet_aton("172.16.60.1", &arguments.dfe_ip_bot);
	arguments.port_bot = 5008;

	arguments.latency = 0;
	arguments.wait = 10000;
	arguments.send = 200;
	arguments.csv = 0;

	//Parse the arguments
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	int sock_top = create_socket(&arguments.dfe_ip_top, arguments.port_top);
	int sock_bot = create_socket(&arguments.dfe_ip_bot, arguments.port_bot);

	send_frames(sock_top, sock_bot, &arguments);

	return 0;
}

