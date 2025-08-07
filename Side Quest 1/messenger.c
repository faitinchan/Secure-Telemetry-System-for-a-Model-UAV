#include <stdio.h>

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
	printf("Listener mode\n");
}

void run_sender(char* dest_ip, char* msg) {
	printf("Sender mode\n");
}
