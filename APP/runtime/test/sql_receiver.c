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


#include <sqlite3.h>

#define RECEIVER_PORT 10000
#define UNITS_PER_SECOND 1000000

static struct sockaddr_in sockaddr;

typedef struct payload_s {
	uint32_t data;
	uint32_t id;
} __attribute__ ((__packed__)) payload_t;



// Program documentation
static char doc[] = "NetworkLatency receiver -- a program to receive and display the output of the "  \
                    "Maxeler NetworkLatency bitstream application";
static char args_doc[] = "DATABASE_FILE LISTEN_IP_ADDR";

// The options the program accepts
static struct argp_option options[] = {
		{"port_bot",	't',	"PORTBOT",	0,	"Port number to use for the bottom DFE network port.", 0 },

		{"friendly",	'f',	0,			0,	"Output in a friendly, human-readable format instead of the default CSV.", 0 },
		{ 0, 0, 0, 0, 0, 0 }
};

//used by main() to communicate with parse_opt()
struct arguments {
	struct in_addr cpu_ip_bot;
	int port_bot;
	int friendly;
	char *database;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch (key) {
//		case 'c':
//			inet_aton(arg, &arguments->cpu_ip_bot);
//			break;
		case 't':
			arguments->port_bot = atoi(arg);
			break;
		case 'f':
			arguments->friendly = 1;
			break;
		case 'd':
			arguments->database = arg;
			break;
		case ARGP_KEY_ARG:
			if (state->arg_num >= 2)
				argp_usage(state);

			if (state->arg_num == 0)
				arguments->database = arg;
			else
				inet_aton(arg, &arguments->cpu_ip_bot);
			break;
		case ARGP_KEY_END:
			if (state->arg_num < 2)
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

static int init_database(sqlite3 *db) {
	char *sql;
	int rc;
	sqlite3_stmt *res;

	sql = "CREATE TABLE IF NOT EXISTS received ("  \
	      "id INTEGER NOT NULL, "  \
	      "latency INTEGER NOT NULL, "  \
	      "seconds REAL NOT NULL"  \
	      ");";
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if( rc != SQLITE_OK ){
		fprintf(stderr, "Error preparing SQL: %s\n", sql);
		return(1);
	}

	rc = sqlite3_step(res);
	sqlite3_finalize(res);
	if( rc != SQLITE_DONE ){
		fprintf(stderr, "SQL error creating received table: %s\n", sql);
		return(1);
	}

	return(0);
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

static void recv_frames(int sock, int friendly, sqlite3 *db) {
	//run 10 inserts at once for the speed up it brings
	static const char SQL[] = "INSERT INTO received (id, latency, seconds) SELECT ? AS id, ? AS latency, ? AS seconds\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?\n"  \
	                          "UNION ALL SELECT ?, ?, ?;";

	payload_t received_frame;

	int rc;
	sqlite3_stmt *res;

	if (db) {
		rc = init_database(db);
		if (rc) {
			return;
		}
		rc = sqlite3_prepare_v2(db, SQL, -1, &res, 0);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error preparing SQL\n");
			return;

		}
	}

	int sn = 0;
	while (true) {
		int now = recv(sock, &received_frame, sizeof(payload_t), 0);
		if (now < 0) {
			fprintf(stderr, "recvfrom error: %s\n", strerror(errno));
			exit(1);
		}

		double latency = ((double) received_frame.data) / UNITS_PER_SECOND;

		if (db) {
			sqlite3_bind_int(res, sn+1, received_frame.id);
			sqlite3_bind_int(res, sn+2, received_frame.data);
			sqlite3_bind_double(res, sn+3, latency);
			sn += 3;
			if (sn == 30) {
				rc = sqlite3_step(res);
				if (rc != SQLITE_DONE) {
					fprintf(stderr, "Error executing SQL for id %" PRIu32 "\n", received_frame.id);
					exit(1);
				}
				sn = 0;
				sqlite3_reset(res);
			}
		}
		if (friendly) {
			printf("Received ID #%08" PRIu32 " with %" PRIu32 " (%gs) latency.\n", received_frame.id, received_frame.data, latency);
		} else {
			printf("'R',%08" PRIu32 ",%" PRIu32 ",%g\n", received_frame.id, received_frame.data, latency);
		}
		fflush(stdout);
	}
}

int main(int argc, char *argv[]) {
	struct arguments arguments;
	//Defaults
	arguments.port_bot = 5008;
	arguments.friendly = 0;
	arguments.database = 0;

	//Parse the arguments
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	int mySocket = create_socket(&arguments.cpu_ip_bot, arguments.port_bot);

	int rc;
	sqlite3 *db = 0;
	if (arguments.database) {
		rc = sqlite3_open(arguments.database, &db);
		if (rc) {
			fprintf(stderr, "Could not open sqlite3 database %s\n", sqlite3_errmsg(db));
			exit(1);
		}
	}

	recv_frames(mySocket, arguments.friendly, db);

	if (arguments.database) {
		sqlite3_close(db);
	}

	return 0;
}

