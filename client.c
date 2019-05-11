/*
  Aarón Alejandro García Guzmán   A01039597
  MISION # 6
  Client program that monitors a specific directory, it detects
  modifications, new files, or deleted files. Once it detects
  something it updates the server with it.
*/

// LIBRARIES
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>


// FLAGS / DEFNITIONS
#define PORT 5000
#define MAXLINE 1024

char *SERV_ADDRESS ="127.0.0.1";

char *NEW_FILE_FLAG = "newFile";
char *DELETE_FILE_FLAG = "deleteFile";
char *END_FLAG = "++++++++++++====++++++++++++END";




// DATA STRUCTURES
struct FileObject {

  FILE *file;
  int fd;
  char name[256];
  time_t oldTime;

};

struct Node {
  struct Node* next;
  struct FileObject *file;

};

struct List {
  int size;
  struct Node* root;

};

// LIST methods
void pushBackNode( struct List *list, struct Node *node );
void deleteNode( struct List *list, char *fileName );
void printList( struct List *list , char *listTitle );
void initList( struct List *list );
int getNode( struct List *list, char *fileName, struct Node *n );
int getFile( struct List *list, char *fileName, struct FileObject **file );
int addFileToList( struct List *directoryFilesList, char *fileName,
                  time_t actualModificationTime );


// NODE methods
void initNode( struct Node *node, struct FileObject *file );
void printFile( struct FileObject *file );


// CLIENT/SERVER methods
int createConnection();
int closeConnection();
int sendMethod( char *method );
int sendFileName( char *fileName );
int sendFile( char *fileName, char *path );


// OTHER methods
int getPath();
int fileGotModified( struct FileObject *existentFile, time_t actualModificationTime );
int updateFileModificationTime( struct FileObject *file, time_t oldTime );
int fileExists( struct List *list, char *fileName, struct Node *node );
int checkIfFilesGotDeleted( struct List *list, struct List *directoryFilesList );
void initFile( struct FileObject *file, char *fileName, time_t actualModificationTime );


// UTILITY
void logger(char *msg);


// GLOBAL VARIABLES
DIR *d;
struct dirent *dir;
char *folder = "/folderMonitoreado/";       // Folder I'm going to monitor
char path[PATH_MAX];

int sockfd;                           // Sever socket file descriptor
int n;                                // Size of bytes in communications
struct sockaddr_in servaddr;          // Socket address



// MAIN EXECUTION
int main( int argc, char *argv[] ) {
  int err;
  struct List *list = malloc(sizeof(*list));
  initList(list);

  // connect to SERVER
  if((err = createConnection()) < 0) {
    printf("Error connecting to server(%s:%d)\n", SERV_ADDRESS, PORT);
    return -1;
  } else {
    printf("Connection to server (%s:%d) established.", SERV_ADDRESS, PORT);
  }


  // get working path
  getPath();

  // append folder we want to monitor for changes to path
  strcat(path, folder);

  printf("Monitored folder path:\n%s\n\n", path);
  int z = 0;
  while(z<20){
    printf("-----------------------------------------------------------\n");
    sleep(5);
    z++;

            // get current DIR
            d = opendir(path);

            if (d) {
              char filePath[1024];                                    // each file path
              struct stat *file_stat = malloc(sizeof(*file_stat));    // file stats variable
              int pathSize = strlen(path);                            // get size of working dir path
              struct List *directoryFilesList = malloc(sizeof(*directoryFilesList)); // Actual iteration Directory list

              initList(directoryFilesList);

              strcpy(filePath, path);

              printf("Files:\n");

              // while not at the end of directory
              while ((dir = readdir(d)) != NULL) {
                struct FileObject *existentFile = malloc(sizeof(*existentFile));
                struct Node *existentNode = malloc(sizeof(*existentNode));
                time_t actualModificationTime;

                char fileName[256];                                   // current file name

                strcpy(fileName, dir->d_name);

                // Ignore curr dir and parent dir
                if( strcmp(".", fileName) == 0 || strcmp("..", fileName) == 0 ) {
                  continue;
                }

                strcat(filePath, fileName);

                // get stats of current file
                err = stat(filePath, file_stat);

                if( err == 0 ) { // file has no errors
                  printf("FILE: \"%s\"\n", fileName);
                  actualModificationTime = file_stat->st_mtime;

                  // add it to directoryFilesList (list of this iteration)
                  printf("Adding to directoryFilesList.\n");
                  addFileToList(directoryFilesList, fileName, actualModificationTime);
                  printf("Success.\n");
                } else {

                  printf("Error(%d) couldn't access stats from file:\"%s\".\n", err, fileName);
                  //continue; // Verify if continue is needed or not.
                }

                // TO-DO
                if( fileExists(list, fileName, existentNode) ) {

                  int err2;
                  // Send reference of pointer in order to modify its value from the function
                  err2 = getFile(list, fileName, &existentFile);

                  if( fileGotModified( existentFile, actualModificationTime == 0 ) ) {
                    printf("File got modified.\n");
                    // Update local list
                    updateFileModificationTime(existentFile, actualModificationTime);
                    // Update server with modified file
                    sendMethod( NEW_FILE_FLAG );
                    sendFileName( fileName );
                    sendFile( fileName, filePath );
                  }

                } else { // File just got created, or if it doesnt it still enters
                        // I could check if file is in glboal list and to not enter here
                  // Update local list
                  addFileToList(list, fileName, actualModificationTime);
                  // UPDATE() server with new file
                  sendMethod( NEW_FILE_FLAG );
                  sendFileName( fileName );
                  sendFile( fileName, filePath );

                }

                if(err == 0) printf("\tMod time: %lld\n", (long long)file_stat->st_mtime);
                else printf("Error in stat().\n");

                filePath[pathSize] = '\0';
              }

              // Now check if any file gotDeleted, by comparing list files with directoryFilesList
              checkIfFilesGotDeleted(list, directoryFilesList);
              printList(directoryFilesList, "Actual iteration list: ");
              free(directoryFilesList);
              free(file_stat);
              closedir(d);
            }

            printList(list, "Local list: ");
  }

  if( closeConnection() < 0 ) {
    printf("Error closing connection to server.\n");
    return -1;
  }

  printf("Succesfuly closed connection to server (%s:%d);\n", SERV_ADDRESS, PORT);

  return(0);

}

// MARK - CLIENT/SERVER methods

// Closes established connection with server
int closeConnection() {

  // close the socket file descriptor
	return close(sockfd);


}

// Establishes connection with server
int createConnection() {

  // clear servaddr
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_addr.s_addr = inet_addr(SERV_ADDRESS);  // server address
	servaddr.sin_port = htons(PORT);                     // global port
	servaddr.sin_family = AF_INET;                       // udp

  // create datagram socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  // connect to server
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		printf("\n Error : Connect Failed \n");
		return -1;
	}

  // returns 0 = success
  return 0;
}

// Send method to server
int sendMethod( char *method ){

  // Send method
  sendto(sockfd, method, strlen(method), 0, (struct sockaddr*)NULL, sizeof(servaddr));

  return 0;

}

// Send fileName to server
int sendFileName( char *fileName ){

  printf("Sending %lu bytes.\n", strlen(fileName));
  // Send file name
	sendto(sockfd, fileName, strlen(fileName), 0, (struct sockaddr*)NULL, sizeof(servaddr));

  return 0;

}

// Send file to server
int sendFile( char *fileName, char *path ){

  char buffer[MAXLINE];

	// open file descriptor read-only
	int fd = open(path, O_RDONLY);

  // While you keep reading lines from file
	while ((n = read(fd, buffer, MAXLINE)) > 0) {
		// request to send datagram
		// no need to specify server address in sendto
		// connect stores the peers IP and port
		sendto(sockfd, buffer, n, 0, (struct sockaddr*)NULL, sizeof(servaddr));
	}
	// Send END_FLAG telling you are done sending file
	sendto(sockfd, END_FLAG, strlen(END_FLAG), 0, (struct sockaddr*)NULL, sizeof(servaddr));

  return 0;

}


// Check if files got Deleted
int checkIfFilesGotDeleted( struct List *list, struct List *directoryFilesList ) {

  struct Node *currentDirNode;
  struct Node *localListNode = list->root;

  char buffer[MAXLINE];

  // Tell server you will start deleting files process
  sendMethod(DELETE_FILE_FLAG);

  // Check if each globalListNode is still in directoryFilesList
  while( localListNode != NULL ){ // Iterate through globalList

    char *nameOfLocalNode = localListNode->file->name;

    // If return num is 0, file got deleted from directoryFilesList
    if( !getNode(directoryFilesList, nameOfLocalNode, currentDirNode) ){
      struct Node *tmpNodeHolder = localListNode -> next;

      // print file to be deleted
      printf("I WILL DELETE THIS FILE: %s\n", nameOfLocalNode);
      printf("I WILL DELETE THIS FILE: %s\n", nameOfLocalNode);
      printf("I WILL DELETE THIS FILE: %s\n", nameOfLocalNode);
      printf("I WILL DELETE THIS FILE: %s\n", nameOfLocalNode);
      printf("I WILL DELETE THIS FILE: %s\n", nameOfLocalNode);

      // delete file from local list
      deleteNode(list, nameOfLocalNode);
      localListNode = tmpNodeHolder;

      // send server fileName to delete
      sendFileName( nameOfLocalNode );

      // receive confirmation of deletion before continueing
      // waiting for response
    	n = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr*)NULL, NULL);
    	buffer[n] = '\0';
    	printf("Server says: File was \"%s\" deleted.\n", buffer);

    } else { // File still there
      localListNode = localListNode->next;
    }

  }

  // Tell server to stop deleting file process
  sendMethod(END_FLAG);

  return 0;
}


// Verifies if time got modified
int fileGotModified( struct FileObject *existentFile, time_t actualModificationTime ) {

  time_t oldTime = existentFile->oldTime;
  time_t delta = actualModificationTime - oldTime;

  if( delta > 0 ) { // file got modified
    return 0;
  } else { // file has not been modified
    return 1;
  }

}


// Updates file time
int updateFileModificationTime( struct FileObject *file, time_t oldTime ) {

  file->oldTime = oldTime;
  return 0;

}

// Initializes file with its' parameters
void initFile(struct FileObject *file, char *fileName,
              time_t actualModificationTime) {

  // assign file to File f
  strcpy(file->name, fileName);
  file->oldTime = actualModificationTime;
  logger("File initialized.");

}

// Returns 1 if file exists, 0 if not
int fileExists(struct List *list, char fileName[], struct Node *node) {
  struct Node *n;

  // Iterate whole list
  if( getNode(list, fileName, n) ) { // found node
      node = n;
      return 1;
  }

  // Didn't find node
  return 0;
}

// Get current working directory
int getPath(){

  if (getcwd(path, sizeof(path)) != NULL) {
      printf("Current working dir:\n%s\n\n", path);
  } else {
      perror("getcwd() error\n");
      return -1;
  }

  return 0;

}



// MARK - LIST METHODS
// Initializes list
void initList( struct List *list ) {

  list->root = NULL;
  list->size = 0;
  logger("List initialized.");

}


// inits File, inits Node and adds it to desired List
int addFileToList(struct List *directoryFilesList, char *fileName,
                  time_t actualModificationTime) {

  struct FileObject *newFile = malloc(sizeof(*newFile));
  struct Node *newNode = malloc(sizeof(*newNode));

  initFile(newFile, fileName, actualModificationTime);
  initNode(newNode, newFile);
  pushBackNode(directoryFilesList, newNode);

  return 0;

}


// Appens node to list
void pushBackNode( struct List *list, struct Node *node ) {

  struct Node* n = list->root;

  if( n == NULL ) { // No node exists yet
    list->root = node;
    list->size++;
    return;
  }

  while( n->next != NULL ){ // Go to last node
    n = n->next;
  }

  // Append it
  n->next = node;
  list->size++;

}

// Get node, returns 1 if found, 0 if not
int getNode(struct List *list, char *fileName, struct Node *n) {

  struct Node* node = list->root;

  // Iterate whole list
  while( node != NULL ){

    char currNodeFileName[256];
    strcpy(currNodeFileName, node->file->name);

    if( strcmp(currNodeFileName, fileName) == 0 ) { // Found node with same name
      n = node;
      return 1;
    }

    node = node->next;

  }

  return 0;
}

// Deletes node of specified index (might change to file name)
void deleteNode( struct List *list, char *fileName ) {

  int i = 0;

  // Check for out of bounds
  if( list->root == NULL ) {
    fprintf(stderr, "Empty list, can't delete.\n");
    return;
  }

  struct Node* n = list->root;
  struct Node* holder = list->root;

  // Node to delete is root
  if( fileName == n->file->name && i == 0 ) {
    list->root = n->next;
    fprintf(stdout, "Sucefully deleted: ");
    printFile(n->file);
    free(n);
    list->size--;
    return;
  }

  // Place yourself before nodeToBeDeleted
  while( n->next != NULL && n->next->file->name != fileName ){
    n = n->next;
    i++;
  }

  // Didn't found node
  if( n->next == NULL ) {
    return;
  }

  // Found node, delete it and free it
  holder = n->next;
  n->next = n->next->next;
  fprintf(stdout, "Sucefully deleted: ");
  printFile(holder->file);
  free(holder);
  list->size--;

}

// Get fileObject
int getFile(struct List *list, char *fileName, struct FileObject **file) {

  struct Node *node;
  int err = getNode(list, fileName, node);

  if( err ) { // No error, 1 == success in this case
    *file = node->file;
    printf("getFile() succesfully ran, file: %s\n", (**file).name);
  } else {
    printf("getNode() inside getFile() failed.\n");
  }

  return err;
}

// Prints entire list
void printList( struct List *list , char *listTitle ) {

  struct Node* node = list->root;
  int i = 0;

  if( node == NULL ) {
    fprintf(stdout, "GG. Empty list.\n");
    return;
  }

  fprintf(stdout, "--------------------------------------------\n");
  fprintf(stdout, "%s\n", listTitle);
  fprintf(stdout, "Files List Size: %d\n", list->size);

  while( node != NULL ) {

    //fprintf(stdout, "Position = %d\n", i);
    printf("%s->", node->file->name);
    node = node->next;
    i++;

  }

  printf("\n");

}

// MARK - NODE METHODS

// Node Constructor
void initNode( struct Node *node, struct FileObject *file ) {

  node->next = NULL;
  node->file = file;
  logger("Node initialized.");

}

// Prints File Name
void printFile( struct FileObject *file ) {

  printf("%s\n", file->name);

}

// MARK - UTILITY
void logger(char *msg) {
  printf("%s\n", msg);
}
