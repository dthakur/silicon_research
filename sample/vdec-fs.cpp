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
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <map>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
#include <fstream>

#define BUFFER_SIZE 512 * 512

#define MSG_TYPE_CONTENT 0x0
#define MSG_TYPE_FRAGMENT 0x1

#pragma pack(1)
struct FragmentHeader {
  uint16_t type;
  uint16_t sequence;
  uint16_t frame_id;
  uint16_t total_count;
	uint16_t fragment_size;
};

struct ContentHeader {
  uint16_t type;
};

std::map<uint16_t, std::vector<std::shared_ptr<uint8_t[]>>> fragment_map;

void process_content(const uint8_t* message, ssize_t size) {
	auto now = std::chrono::system_clock::now();
	auto now_time_t = std::chrono::system_clock::to_time_t(now);
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::tm now_tm = *std::localtime(&now_time_t);
	char filename[64];
	std::strftime(filename, sizeof(filename), "%Y%m%d%H%M%S", &now_tm);
	sprintf(filename + strlen(filename), "%03d", static_cast<int>(now_ms.count()));

	std::ofstream outfile(filename, std::ios::binary);
	outfile.write(reinterpret_cast<const char*>(message), size);
	outfile.close();
}

void process_message(const uint8_t* message, ssize_t size);

void process_fragment(const uint8_t* message, ssize_t size) {
	struct FragmentHeader *header = (struct FragmentHeader *)message;
	auto &fragments = fragment_map[header->frame_id];

	// printf("fragment received frame_id=%d sequence=%d total=%d\n", header->frame_id, header->sequence, header->total_count);
	
	// NOTE: code does not deal with garbage data [bad frame_ids, dos etc]
	
	auto fragment = std::shared_ptr<uint8_t[]>(new uint8_t[size]);
	memcpy(fragment.get(), message, size);
	fragments.push_back(fragment);

	if (fragments.size() != header->total_count) {
		return;
	}

	std::sort(fragments.begin(), fragments.end(), [](const std::shared_ptr<uint8_t[]> &a, const std::shared_ptr<uint8_t[]> &b) {
		const FragmentHeader *header_a = (const FragmentHeader *)a.get();
		const FragmentHeader *header_b = (const FragmentHeader *)b.get();
		return header_a->sequence < header_b->sequence;
	});

	// calculate total size
	size_t total_size = 0;
	for (const auto &fragment : fragments) {
		const FragmentHeader *header = (const FragmentHeader *)fragment.get();
		total_size += header->fragment_size;
	}

	auto complete_message = std::shared_ptr<uint8_t[]>(new uint8_t[total_size]);
	uint8_t *ptr = complete_message.get();
	for (const auto &fragment : fragments) {
		const FragmentHeader *header = (const FragmentHeader *)fragment.get();
		memcpy(ptr, fragment.get() + sizeof(FragmentHeader), header->fragment_size);
		ptr += (size - sizeof(FragmentHeader));
	}

	process_content(complete_message.get(), total_size);
	fragment_map.erase(header->frame_id);
}

void process_message(const uint8_t* message, ssize_t size) {
	assert(size > 2);
	uint16_t type = *((uint16_t*)message);

	// printf("message received type=%d size=%zd\n", type, size);
	if (type == MSG_TYPE_CONTENT) {
		process_content(message + sizeof(ContentHeader), size - sizeof(ContentHeader));
	} else if (type == MSG_TYPE_FRAGMENT) {
		process_fragment(message, size);
	} else {
		printf("unknown type of message %d", type);
		return;
	}
}

int main(int argc, const char *argv[]) {
	int rtp_port = 5000;

	if (argc > 1) {
		rtp_port = atoi(argv[1]);
	}

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(rtp_port);
	address.sin_addr.s_addr = INADDR_ANY;

	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	bind(udp_sock, (struct sockaddr*)&address, sizeof(struct sockaddr_in));

	uint8_t rx_buffer[BUFFER_SIZE];

	while (true) {
		ssize_t rx_length = recv(udp_sock, rx_buffer, BUFFER_SIZE, 0);

		if (rx_length < 0) {
			printf("error in recvfrom rx_length=%zd\n", rx_length);
			return -1;
		}

		process_message(rx_buffer, rx_length);
	}

	return 0;
}
