// server program for udp connection
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define PORT 5000
#define MAXLINE 1024

char *NEW_FILE_FLAG = "newFile";
char *DELETE_FILE_FLAG = "deleteFile";

char *END_FLAG = "++++++++++++====++++++++++++END";

// Driver code
int main(){

	char buffer[MAXLINE], method[MAXLINE], fileName[MAXLINE];
	int listenfd, len, n, fd, err;
	struct sockaddr_in servaddr, cliaddr;
	bzero(&servaddr, sizeof(servaddr));

	// Create a UDP Socket
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	servaddr.sin_family = AF_INET;

	// bind server address to socket descriptor
	bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	//receive the datagram
	len = sizeof(cliaddr);

	while(1) {

							// Receive method
							n = recvfrom(listenfd, method, MAXLINE,
									0, (struct sockaddr*)&cliaddr,(socklen_t *)&len); //receive message from server
							method[n] = '\0';
							printf("Method: %s\n", method);

							// new file or update file
							if(strcmp(NEW_FILE_FLAG, method) == 0) {

											// Receive file name
											n = recvfrom(listenfd, fileName, MAXLINE,
													0, (struct sockaddr*)&cliaddr,(socklen_t *)&len); //receive message from server
											fileName[n] = '\0';
											printf("FileName: %s\n", fileName);

											// Get fd of created file
											fd = open(fileName, O_RDWR | O_CREAT, 0666);

											// Receive file and create it
											while( (n = recvfrom(listenfd, buffer, MAXLINE, 0,
														(struct sockaddr*)&cliaddr,(socklen_t *)&len)) > 0 ) {

															buffer[n] = '\0';
															if( strcmp(END_FLAG,buffer) == 0 ) { //end of file reached
																break;
															}
															write(fd, buffer, n);

											}

											close(fd); // Close file

							} else if(strcmp(DELETE_FILE_FLAG, method) == 0) { // delete local file
								// delete file
								printf("I'm here.\n");

								// Receive fileNames to delete
								while( (n = recvfrom(listenfd, fileName, MAXLINE,
											 0, (struct sockaddr*)&cliaddr,(socklen_t *)&len)) > 0 ){ //receive message from server
									fileName[n] = '\0';

									printf("FileName: %s\n", fileName);

									// Stop removing files when you see END FLAG
									if(strcmp(fileName, END_FLAG) == 0){
										break;
									}

									// NO ESTA ENTRANDO AQUI POR ALGUNA RAZON
									// Else Delete file
									err = remove(fileName);
									if( err == 0 ) { // Enviar confirmacion que se borro
										printf("File \"%s\" succesfully removed.\n", fileName);
										strcat(buffer,"Succesfully");
									} else {
										printf("File \"%s\" was already removed or doesn't exist.\n", fileName);
										strcat(buffer,"NOT Succesiffuly");
									}

									// Send confirmation or no confirmation message to client, response
    							sendto(listenfd, buffer, strlen(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));

								}


							}
		}


	// send the response
	char *message = "Thank you";
	sendto(listenfd, message, MAXLINE, 0,
		(struct sockaddr*)&cliaddr, sizeof(cliaddr));
}
