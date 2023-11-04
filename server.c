#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

pthread_mutex_t mutex; 	// Creating a mutex

#define MAX_NUM_CLIENTS 3	

void* whatsapp(void* client_socket);	//The thread function that waits for a clients message and then sends it to all the users

struct clients_data{	
	int client_socket;
	char buffer[1024];
};

int server_sockfd;	// Servers socket

int client_count_thread;// Counts how many clients are connected

int list_of_client_ports[MAX_NUM_CLIENTS];	// an array that holds the socketfd of each client


int main(void){	
	int client_sockfd;	// Clients socket
	struct sockaddr_in server_addr, client_addr;		// Creating an address for the server and client
	socklen_t server_addr_len = sizeof(server_addr);	// Used in bind and accept func. You can use sizeof but it throws err.
	socklen_t client_addr_len = sizeof(client_addr);	// Used in bind and accept func. You can use sizeof but it throws err.
	pthread_t thread_num[MAX_NUM_CLIENTS];	// Creating threads
	
	puts("SERVER: Start");
	
	
	struct clients_data* clients_array[MAX_NUM_CLIENTS]; // Creating an array of structs to hold the clients data
	if(clients_array == NULL){
		perror("Clients_array didnt intialized");
		exit(EXIT_FAILURE);
	}
	
	pthread_mutex_init(&mutex, NULL);	// Initializing mutex
	
	//Creating the server socket
	if((server_sockfd = socket( AF_INET, SOCK_STREAM, 0)) == -1){ 
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	//Seting the servers atributes like: address, port number and who he will ne listening to.  
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; 			// Using IPv4
	server_addr.sin_port =  htons(55152);		// Port number 55152
	server_addr.sin_addr.s_addr = INADDR_ANY; 	// Listens to conect to any available network
	
	
	// Binding the socket of the server and its network values.
	if(bind(server_sockfd, (struct sockaddr*) &server_addr, server_addr_len) == -1){
		perror("bind");
		close(server_sockfd);
		exit(EXIT_FAILURE);
	}
	
	// Listening to a packet from a client
	if(listen(server_sockfd, 5) == -1){
		perror("listen");
		exit(EXIT_FAILURE);
	}

	int client_count = 0;
	
	for(client_count; client_count < MAX_NUM_CLIENTS; client_count++){
		
		clients_array[client_count] = malloc(sizeof(struct clients_data));
		
		// Intializing the clients socketfd to a unique struct with its own memory 
		if((clients_array[client_count]->client_socket = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1){
			perror("accept");
			close(clients_array[client_count]->client_socket);
			free(clients_array[client_count]);
			exit(EXIT_FAILURE);
		}
		
		pthread_mutex_lock(&mutex); // If another client connects while on of the existing clinets sends a message, it wont change the value of client_count mid sending.
		
		// Creating a new thread to each new client
		if(pthread_create(&thread_num[client_count], NULL, whatsapp, clients_array[client_count]) == -1){
				perror("pthread_creat");
				free(clients_array[client_count]);
				close(clients_array[client_count]->client_socket);
		}
		// Storing the clients socketfd in an array. This array will help us to know where to send the messages
		list_of_client_ports[client_count] = clients_array[client_count]->client_socket;
		
		// Averibale that will be used globaly 
		client_count_thread = client_count;
		
		pthread_mutex_unlock(&mutex);
	}
	
	// Waiting for all the threads to join
	for(int i = 0; i < client_count; i++){
		if(pthread_join(thread_num[i], NULL) == -1){
			perror("pthread_join");
			return 1;
		}
	}
	
	pthread_mutex_destroy(&mutex);
	if(close(server_sockfd) == -1){ // If an error accures we closeing the socket
		perror("close server_sockfd");
		exit(EXIT_FAILURE);
	}
	puts("Server quits");
	pthread_mutex_destroy(&mutex);
	return 0;
}









void* whatsapp(void* arg){
	
	struct clients_data* client = (struct clients_data*) arg;
	
	char num_client_apear[25];	// Will contain the clients number
	char temp[2];	//Used to store the number of the client converted from integer to string
	strcpy(num_client_apear,"User_");	
	sprintf(temp,"%d", client->client_socket); // Changing the integer to a char array type and storing the value in temp 
	strcat(num_client_apear, temp);	// output: User_{value of temp}. For Example: temp = 4 => User_4
	printf("%s socket number: %d\n", num_client_apear,client->client_socket); // Print to the servers terminal the value of socketfd of the client
	
	while(1){
		int bytes_recived = recv(client->client_socket , client->buffer, sizeof(client->buffer), 0);	// Waits for a data from the client who this thread was assigned to
		if( bytes_recived == -1){
			perror("recv");
			if(close(client->client_socket) == -1){ // If an error accures we are closeing the socket
				perror("close");
				exit(EXIT_FAILURE);
			}	
			pthread_exit(NULL);
		}
		
		client->buffer[bytes_recived] = '\0'; 
		
		pthread_mutex_lock(&mutex);	// While sending a message to other users there shouldnt be any chance for race condition
		
		if((strcmp(client->buffer, "q") == 0) || (strcmp(client->buffer, "Q") == 0 || strcmp(client->buffer, "") == 0)){ //Check if the client wants to leave the chatroom
			strcat(num_client_apear, " left the chat");
			printf("%s\n", num_client_apear);
			if(close(client->client_socket) == -1){ // If the client leaves the chat close its socket
				perror("close");
				pthread_mutex_unlock(&mutex);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&mutex);
			pthread_exit(NULL);
		}
		
		printf("%s\n", client->buffer); // Show the server what was printed
		
		
		
		// Sends the message from one client to all of them
		for(int i = 0; i < client_count_thread + 1; i++){
			// Prints to the client the number of user. I know I can do it with the loop below but I want it to apear above the message
			int send_bytes = send(list_of_client_ports[i], num_client_apear, sizeof(num_client_apear), 0);
			if( send_bytes == -1){
				perror("send");
				if(close(client->client_socket) == -1){ // If an error accures we closeing the socket
					perror("close");
					pthread_mutex_unlock(&mutex);
					pthread_exit(NULL);
				}
				pthread_mutex_unlock(&mutex);
				pthread_exit(NULL);
			}
			
			
			//Prints to the client the message
			send_bytes = send(list_of_client_ports[i], client->buffer, sizeof(client->buffer), 0);
			if( send_bytes == -1){
				perror("send");
				if(close(client->client_socket) == -1){ // If an error accures we closeing the socket
					perror("close");
					pthread_mutex_unlock(&mutex);
					pthread_exit(NULL);
				}
				pthread_mutex_unlock(&mutex);
				pthread_exit(NULL);
			}
			printf("Message was sent to client%d\n", i);
		} 
		pthread_mutex_unlock(&mutex);
	}
	
	// We will never get here
	if(close(client->client_socket) == -1){ // If an error accures we closeing the socket
		perror("close");
		pthread_mutex_unlock(&mutex);
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}
