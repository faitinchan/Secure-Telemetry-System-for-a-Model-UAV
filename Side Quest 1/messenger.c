#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define MAX_PASSWORD_LEN 64

void run_listener();
void run_sender(char* dest_ip, char* msg);
char* get_password();

int main(int argc, char* argv[]) {
	if (argc == 1) {
		char* password = get_password();

		printf("[*] Listening for messages...\n");
		run_listener();

		free(password);
	} else if (argc == 3) {
		char* password = get_password();

		char* dest_ip = argv[1];
		char* msg = argv[2];
		printf("[*] Sending message to %s: \"%s\"\n", dest_ip, msg);
		run_sender(dest_ip, msg);

		free(password);
	} else {
		printf("Usage:\n");
		printf("  %s                   # Listen for messages\n", argv[0]);
		printf("  %s <IP> <message>    # Send message to IP\n", argv[0]);
		return 1;
	}

	return 0;
}

void run_listener() {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Socket creation failed\n");
		return;
	}

	struct sockaddr_in listen_addr;
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	listen_addr.sin_port = htons(12345);

	if (bind(sockfd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
		printf("Bind failed\n");
		close(sockfd);
		return;
	}

	printf("[*] Listening on port 12345...\n");

	struct sockaddr_in sender_addr;
	char buffer[1024];
	socklen_t sender_len = sizeof(sender_addr);
	while (1) {
		ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);
		if (recv_len < 0) {
			printf("Receive data on the socket failed\n");
			break;
		}

		buffer[recv_len] = '\0';

		char sender_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);

		printf("[+] Received from %s: \"%s\"\n", sender_ip, buffer);
	}

	close(sockfd);
}

void run_sender(char* dest_ip, char* msg) {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Socket creation failed\n");
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
		printf("[-] sendto failed\n");
	} else {
		printf("[+] Message sent (%ld bytes)\n", sent_bytes);
	}

	close(sockfd);
}

char* get_password() {
	char* pwd = malloc(MAX_PASSWORD_LEN + 1);
	if (!pwd) {
		printf("Fail to allocate buffer for password.\n");
		return NULL;
	}

	struct termios oldt, newt;

	if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
		printf("Fail to get current terminal setting.\n");
		free(pwd);
		return NULL;
	}

	newt = oldt;
	newt.c_lflag &= ~(ECHO);

	if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) {
		printf("Fail to apply new terminal setting.\n");
		free(pwd);
		return NULL;
	}

	printf("Enter password (max %d characters): ", MAX_PASSWORD_LEN);
	fflush(stdout);

	if (fgets(pwd, MAX_PASSWORD_LEN + 1, stdin) == NULL) {
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
		printf("Fail to get user input.\n");
		free(pwd);
		return NULL;
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	printf("\n");

	size_t len = strcspn(pwd, "\n");
	if (pwd[len] == '\n') {
		pwd[len] = '\0';
	} else {
		int ch;
		while ((ch = getchar()) != '\n' && ch != EOF);
	}

	return pwd;
}
