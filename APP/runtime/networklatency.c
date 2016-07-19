#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>

#include <errno.h>
#include <err.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <inttypes.h>
#include <limits.h>
#include <argp.h>
#include <netinet/ether.h>

#include "MaxSLiCInterface.h"
#include "MaxSLiCNetInterface.h"

#define RECEIVER_PORT 10000

extern max_file_t *NeworkLatency_init();

// Program documentation
static char doc[] = "NetworkLatency -- a program to measure the latency between two network connections";
static char args_doc[] = "";

// The options the program accepts
static struct argp_option options[] = {
		{"dfe_ip_top",	'D',	"DFEIPTOP",	0,	"IP Address for the top DFE network port.", 0 },
		{"port_top",	'T',	"PORTTOP",	0,	"Port number to use for the top DFE network port.", 0 },
		{"netmask_top",	'N',	"MASKTOP",	0,	"Netmask for the top DFE port's network.", 0 },
		{"dfe_ip_bot",	'd',	"DFEIPBOT",	0,	"IP Address for the bottom DFE network port.", 0 },
		{"port_bot",	't',	"PORTBOT",	0,	"Port number to use for the bottom DFE network port.", 0 },
		{"netmask_bot",	'n',	"MASKBOT",	0,	"Netmask for the bottom DFE port's network.", 0 },
		{"cpu_ip_bot",	'c',	"CPUIPBOT",	0,	"IP Address to listen to.", 0 },

		{ 0, 0, 0, 0, 0, 0 }
};

//used by main() to communicate with parse_opt()
struct arguments {
	struct in_addr dfe_ip_top;
	int port_top;
	struct in_addr mask_top;
	struct in_addr dfe_ip_bot;
	int port_bot;
	struct in_addr mask_bot;
	struct in_addr cpu_ip_bot;
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
		case 'N':
			inet_aton(arg, &arguments->mask_top);
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

uint16_t get_receiver_port(size_t index) {
	return RECEIVER_PORT + index;
}


int main(int argc, char *argv[]) {
	struct arguments arguments;
	//Defaults
	inet_aton("172.16.50.1", &arguments.dfe_ip_top);
	inet_aton("255.255.255.0", &arguments.mask_top);
	arguments.port_top = 5007;
	inet_aton("172.16.60.1", &arguments.dfe_ip_bot);
	inet_aton("255.255.255.0", &arguments.mask_bot);
	arguments.port_bot = 5008;
	inet_aton("172.16.60.10", &arguments.cpu_ip_bot);

	//Parse the arguments
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	max_file_t *maxfile = NetworkLatency_init();

	max_engine_t *engine = max_load(maxfile, "*");

	max_ip_config(engine, MAX_NET_CONNECTION_QSFP_TOP_10G_PORT1, &arguments.dfe_ip_top, &arguments.mask_top);
	max_ip_config(engine, MAX_NET_CONNECTION_QSFP_BOT_10G_PORT1, &arguments.dfe_ip_bot, &arguments.mask_bot);

	max_config_set_bool(MAX_CONFIG_PRINTF_TO_STDOUT, true);

	max_actions_t *actions = max_actions_init(maxfile, "default");

	max_udp_socket_t *socket = malloc(sizeof(max_udp_socket_t*));

	socket = max_udp_create_socket_with_number(engine, "udpBot1", 0);

	max_udp_connect(socket, &arguments.cpu_ip_bot, get_receiver_port(0));


	max_set_uint64t(actions, "kData_A", "port", arguments.port_top);
	max_set_uint64t(actions, "kData_B", "port", arguments.port_bot);
	max_set_uint64t(actions, "kLatency", "socket", 0);

	max_run(engine, actions);


	printf("Running. Press enter to exit.\n");
	getchar();

	max_actions_free(actions);

	max_unload(engine);
	max_file_free(maxfile);

	printf("Done.\n");
	return 0;
}
