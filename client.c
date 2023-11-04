#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>


struct clients_data{
	int client_socket;
	char buffer[1024];
};

void* incoming_message(void* arg); // Function for a thread to listen for incoming message

pthread_mutex_t mutex;

int connection_flag = 0; // Flags that you are connected to the server for the first time

int main(void){
	char buffer[1024];	// Where messages of data will be stored
	struct sockaddr_in server_addr;	// Creating a struct for the server information regarding network
	socklen_t server_addr_len = sizeof(server_addr); // To use later in the connect func
	struct clients_data client;	// Intializing the struct that will hold the information about the clinets socket number and the message data
	pthread_t thread;	// The thread that will run in the background and wait for incoming message from the server
	
	
	pthread_mutex_init(&mutex, NULL);
	
	// Creating the clients socket
	if((client.client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	// Defining the server address
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(55152);
	server_addr.sin_addr.s_addr = inet_addr("192.168.7.2");
	
	// Trying to connect to the server
	if(connect( client.client_socket, (struct sockaddr*)&server_addr, server_addr_len) == -1){
		perror("connect");
		exit(EXIT_FAILURE);
	}
	
	// Creating the thread
	if(pthread_create(&thread, NULL, incoming_message, &client) == -1){
			perror("pthread_creat");
			if(close(client.client_socket) == -1){ // If an error accures we closeing the socket
				perror("close");
				exit(EXIT_FAILURE);
			}	
		}
	puts("You are connected:");	// Apears once when the connection with the sever is successfule  
	
	
	while(1){
		
		// Client waits for the server to send data
		if (fgets(client.buffer, sizeof(client.buffer), stdin)!= NULL){
			// Checks if the buffer( the message we send) is empty
			if(strlen(client.buffer) == 1){
				char x[] = "Didnt enter any text\n";
				strcpy(client.buffer, x);
			}
			// Heandeling any free space after the use of the 'Enter' key
			int len = strlen(client.buffer);
			if (len > 0 && client.buffer[len - 1] == '\n') {
				client.buffer[len - 1] = '\0';	
			}
			else {
				printf("Error reading input.\n");
				if(close(client.client_socket) == -1){ // If an error accures we closeing the socket
					perror("close");
					exit(EXIT_FAILURE);
				}	
			}
		}
			pthread_mutex_lock(&mutex); // The moment the client entered his message
										// it locks the thread in the background and 
										// doesnt allow it to print to the terminal
			
		if((strcmp(client.buffer, "q") == 0) || (strcmp(client.buffer, "Q") == 0)){	// If the client wants to quit pressing 'q' or 'Q' will finish the connection with the server
			
			pthread_cancel(thread);	// Send a signal to the thread to quit imidiatly, since it is in a constant waiting for incoming data using this will not cause memory leak
			
			if (pthread_join(thread, NULL) == -1) { // Waiting to the thread to join after the user wants to quit the chat
				perror("Thread join failed");
				if(close(client.client_socket) == -1){ // If an error accures we closeing the socket
					perror("close");
					exit(EXIT_FAILURE);
				}	
			}
			pthread_mutex_unlock(&mutex);	// When exiting the program unlocking the mutex (I dont thonk it's necessary, but why not)
			pthread_mutex_destroy(&mutex);		
			if(close(client.client_socket) == -1){ // Closing the socket and finishing the session
				perror("close");
				exit(EXIT_FAILURE);
			}	
			puts("You left the chatroom");
			return 0;
		}
		else{
			int send_bytes = send(client.client_socket, client.buffer, sizeof(client.buffer), 0); // If the user doesnt want to quit the message is sent to the server
			if( send_bytes == -1){
				perror("send");
				if(close(client.client_socket) == -1){ // If an error accures we closeing the socket
					perror("close");
					exit(EXIT_FAILURE);
				}		
			}
			
		pthread_mutex_unlock(&mutex);	// After sending the message, the thread in the background can now print to the terminal
		}
	}
	
	// Shouldn't get here
	pthread_mutex_destroy(&mutex);
	if(close(client.client_socket) == -1){
		perror("close");
		exit(EXIT_FAILURE);
	}
	return 0;
}


void* incoming_message(void* arg){
	
	struct clients_data* client = (struct clients_data*) arg;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);	// The thread is constantly listening for a signal from the main thread to exit and it exits immediately 
	while(1){
		if((strcmp(client->buffer, "q") == 0) || (strcmp(client->buffer, "Q") == 0)){	// If the client wants to quit pressing 'q' or 'Q' will finish the connection with the server
			break;
		}
		if(connection_flag == 0){ // When we first connect to the server so ther are know '\n'-s
			connection_flag++;
		}
		else if( strcmp(client->buffer, "") == 0){ // If the buffer is empty more then once, shit hits the fan and the function runs without stoping.
			break;
		}
			int bytes_recived = recv(client->client_socket, client->buffer, sizeof(client->buffer), 0); // Reciving the message data
			if( bytes_recived == -1){
				perror("recv");
				if(close(client->client_socket) == -1){
					perror("close func in thread");
					exit(EXIT_FAILURE);
				}
			}
			client->buffer[bytes_recived] = '\0'; 
		
			pthread_mutex_lock(&mutex);
		
			printf("%s\n", client->buffer); // Prints the message to the terminal
		
			pthread_mutex_unlock(&mutex);
	}
	puts("Server closed the chat");
	puts("Pleas enter the key q to exit the chat room");
	return 0;
}
