/*
 * g++ vdec-fs.c -o vdec-fs -s -Wall
 *
 * Usage:
 * ./vdec-fs 5000 ./images
 *
 */

#include <cstdio>
#include <cstdbool>
#include <cstdlib>
#include <cassert>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <map>
#include <vector>
#include <algorithm>

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

std::map<uint16_t, std::vector<std::string>> fragment_map;

void process_message(const char* message, ssize_t size) {
	assert(size > 2);
	uint16_t type = *((uint16_t*)message);
	if (type == MSG_TYPE_CONTENT) {
		process_content(message, size);
	} else if (type == MSG_TYPE_FRAGMENT) {
		process_fragment(message, size);
	} else {
		printf("unknown type of message %d", type);
		return -1;
	}
}

void process_content(const char* message, ssize_t size) {
}

void process_fragment(const char* message, ssize_t size) {
	struct FragmentHeader *header = (struct FragmentHeader *)message;
	auto &fragments = fragment_map[header->frame_id];
	
	fragments.push_back(std::string(message, size));

	if (fragments.size() == header->total) {
		std::sort(fragments.begin(), fragments.end(), [](const std::string &a, const std::string &b) {
			const FragmentHeader *header_a = (const FragmentHeader *)a.c_str();
			const FragmentHeader *header_b = (const FragmentHeader *)b.c_str();
			return header_a->sequence < header_b->sequence;
		});

		std::string complete_message;
		for (const auto &fragment : fragments) {
			complete_message += fragment.substr(sizeof(FragmentHeader));
		}

		process_message(complete_message.c_str(), complete_message.size());
		fragment_map.erase(header->frame_id);
	}
}

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
		ssize_t rx_length = recv(
			udp_sock,
			rx_buffer,
			BUFFER_SIZE,
			0);

		if (rx_length < 0) {
			usleep(1);
			continue;
		}

		process_message(rx_buffer, rx_length);
	}

	free(rx_buffer);

	return 0;
}
