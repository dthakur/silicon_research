/*
 * gcc vdec-fs.c -o vdec-fs -s -Wall
 *
 * Usage:
 * ./vdec-fs 5000 ./images
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/socket.h>

#define BUFFER_SIZE 512 * 512

#define MSG_TYPE_CONTENT 0x0
#define MSG_TYPE_FRAGMENT 0x1

#pragma pack(1)
struct FragmentHeader {
  uint16_t type;
  uint16_t sequence;
  uint16_t frame_id;
  uint16_t total;
};

struct ContentHeader {
  uint16_t type;
};

int main(int argc, const char *argv[]) {
	int rtp_port = 5000;

	if (argc > 1) {
		rtp_port = atoi(argv[1]);
	}

	// const char *path = "./images";
	// if (argc > 2) {
	// 	path = argv[2];
	// }

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(rtp_port);
	address.sin_addr.s_addr = INADDR_ANY;

	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	bind(udp_sock, (struct sockaddr*)&address, sizeof(struct sockaddr_in));

	char *rx_buffer[BUFFER_SIZE];

	while (true) {
		int rx_length = recv(
			udp_sock,
			rx_buffer,
			BUFFER_SIZE,
			0);

		if (rx_length < 0) {
			usleep(1);
			continue;
		}

		assert(rx_length > 2);
		uint16_t type = *((uint16_t*)rx_buffer);
		if (type == MSG_TYPE_CONTENT) {
			printf("content");
		} else if (type == MSG_TYPE_FRAGMENT) {
			printf("fragment");
		} else {
			printf("unknown type of message %d", type);
			return -1;
		}
		
	}

	free(rx_buffer);

	return 0;
}
