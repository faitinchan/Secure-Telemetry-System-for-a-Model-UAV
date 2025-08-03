#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void run_listener();
void run_sender(char* dest_ip, char* msg);

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printf("[*] Listening for messages...\n");
		run_listener();
	} else if (argc == 3) {
		char* dest_ip = argv[1];
		char* msg = argv[2];
		printf("[*] Sending message to %s: \"%s\"\n", dest_ip, msg);
		run_sender(dest_ip, msg);
	} else {
		printf("Usage:\n");
		printf("  %s                   # Listen for messages\n", argv[0]);
		printf("  %s <IP> <message>    # Send message to IP\n", argv[0]);
		return 1;
	}

	return 0;
}

void run_listener() {
	printf("Listened\n");
	return;
}

void run_sender(char* dest_ip, char* msg) {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Socket creation failed");
		return;
	}

	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(12345);
	if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) {
		printf("Invalid IP address format\n");
		close(sockfd);
		return;
	}

	ssize_t sent_bytes = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (sent_bytes < 0) {
		printf("[-] sendto failed");
	} else {
		printf("[+] Message sent (%ld bytes)\n", sent_bytes);
	}
	
	close(sockfd);
}
