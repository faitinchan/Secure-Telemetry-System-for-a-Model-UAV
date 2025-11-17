#include <arpa/inet.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define MAX_PASSWORD_LEN 64
#define MAX_MSG_LEN 1024

void run_listener();
void run_sender(char* dest_ip, unsigned char* en_msg, size_t total_len);
char* get_password();
unsigned char* encrypt_msg(char* pwd, char* msg, size_t* out_len);

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
		size_t len = strcspn(msg, "\0");
		if (len > MAX_MSG_LEN) {
			msg[MAX_MSG_LEN] = '\0';
		}
		printf("[*] Sending message to %s: \"%s\"\n", dest_ip, msg);

		size_t en_msg_len;
		unsigned char* en_msg = encrypt_msg(password, msg, &en_msg_len);

		run_sender(dest_ip, en_msg, en_msg_len);

		free(password);
		free(en_msg);
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
	char buffer[MAX_MSG_LEN + 1];
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

void run_sender(char* dest_ip, unsigned char* en_msg, size_t total_len) {
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

	ssize_t sent_bytes = sendto(sockfd, (char*)en_msg, total_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
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

unsigned char* encrypt_msg(char* pwd, char* msg, size_t* out_len) {
	if (sodium_init() < 0) {
		printf("Fail to initialize libsodium.\n");
		return 1;
	}

	unsigned char key[crypto_aead_chacha20poly1305_KEYBYTES];
	unsigned char salt[crypto_pwhash_SALTBYTES];
	randombytes_buf(salt, sizeof(salt));

	if (crypto_pwhash(key, sizeof(key), pwd, strlen(pwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) != 0) {
		printf("Out of memory while hashing password.\n");
		return 1;
	}

	unsigned char ciphertext[sizeof(msg) + crypto_aead_chacha20poly1305_ABYTES];
	uint64_t ciphertext_len;

	unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
	randombytes_buf(nonce, sizeof(nonce));

	crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertext_len, msg, sizeof(msg), NULL, 0, NULL, nonce, key);

	size_t total_len = sizeof(ciphertext_len) + sizeof(salt) + sizeof(nonce) + ciphertext_len;
	unsigned char* en_msg = malloc(total_len);

	size_t i = 0;
	memcpy(en_msg + i, &ciphertext_len, sizeof(ciphertext_len));
	i += sizeof(ciphertext_len);
	memcpy(en_msg + i, salt, sizeof(salt));
	i += sizeof(salt);
	memcpy(en_msg + i, nonce, sizeof(nonce));
	i += sizeof(nonce);
	memcpy(en_msg + i, ciphertext, ciphertext_len);

	*out_len = total_len;
	return en_msg;
}
