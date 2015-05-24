#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void display_welcome();
void display_commands();
char get_command();
char *get_input();
void send_request(int sockfd, char *buffer);
char *recv_response(int sockfd, char *response);
char *process_command(char command, int sockfd);
void set_sockaddr(struct sockaddr_in *socket_addr, int port);
void delete(int sockfd, char *response);
void upload(int sockfd, char *response);
bool send_file(int sockfd, char *filename);
bool recv_file(int clientfd, char *filename);
bool send_command(char command, int sockfd);

bool assert_equals(char *actual, char *expected);
void test_list(char *response);
void test_upload(int sockfd, char *response);
void test_download(int sockfd, char *response);
void test_delete(int sockfd, char *response);
void test_quit(char *response);
void test_commands(char command, int sockfd);
void test(int sockfd);

void run_bitdrive(int sockfd);
void start_client(char *server, int port);
void error_occurred(const char *msg);

int main(int argc, char* argv[]){
  if(argc < 3) {
    printf("Usage: %s <ip of server> <port>", argv[0]);
    exit(0);
  }

  display_welcome();
  start_client(argv[1], atoi(argv[2]));
  return 0;
}

void start_client(char *server, int port){
  struct sockaddr_in server_addr;
  struct hostent *host_addr;
  int sockfd, status, n;

  set_sockaddr(&server_addr, htons(port));

  host_addr = gethostbyname(server);
  if(host_addr == NULL){
    error_occurred("ERROR, no such host\n");
    exit(0);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    error_occurred("ERROR opening socket");
  }

  bcopy((char *)host_addr->h_addr,
        (char *)&server_addr.sin_addr.s_addr,
        host_addr->h_length);

  status = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
  if(status < 0){
    error_occurred("ERROR connecting");
  }

  printf("Connected to: %s\n", server);

  // run_bitdrive(sockfd);
  test(sockfd);
  close(sockfd);
}

void set_sockaddr(struct sockaddr_in *socket_addr, int port) {
  bzero((char *) socket_addr, sizeof(*socket_addr));
  socket_addr->sin_family = AF_INET;
  socket_addr->sin_port = port;
}

char *process_command(char command, int sockfd){
  char *buffer;
  char *response;
  int status;

  response = malloc(sizeof(char) * 256);
  bzero(response, 256);

  switch(command){
    case 'L':
      buffer = "LIST";
      break;
    case 'U':
      buffer = "UPLOAD";
      break;
    case 'D':
      buffer = "DOWNLOAD";
      break;
    case 'X':
      buffer = "DELETE";
      break;
    case 'Q':
      buffer = "QUIT";
      break;
    default:
      break;
  }

  if(command != 'V'){
    send_request(sockfd, buffer);
    return recv_response(sockfd, response);    
  }

  return NULL;
}

void send_request(int sockfd, char *buffer){
  int status = write(sockfd, buffer, strlen(buffer));
  if(status < 0) {
    error_occurred("ERROR writing to socket");
  }  
}

char *recv_response(int sockfd, char *response){
  int status = read(sockfd, response, 255); //receive the response
  if(status < 0){
    error_occurred("ERROR reading from socket");
  }

  return response;  
}

void display_welcome(){
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  printf("Welcome to BitDrive! \n");
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

void display_commands(){
  printf("\nCommands:\n");
  printf("==================================================================\n");  
  printf("[L] LIST: List the names of the files currently stored in the server.\n");
  printf("[U] UPLOAD: Upload a file to the server.\n");
  printf("[D] DOWNLOAD: Download a file from the server.\n");
  printf("[X] DELETE: Delete a file from the server.\n");
  printf("[V] VIEW: View list of commands.\n");
  printf("[Q] QUIT: Exit BitDrive.\n\n");
}

bool send_command(char command, int sockfd){
  char *response;
  response = process_command(command, sockfd);

  switch(command){
    case 'L':
      printf("Here are the list of files:\n");
      printf("----------------------------------------------\n"); 
      printf("%s", response);
      printf("----------------------------------------------\n");
      break;
    case 'U':
      upload(sockfd, response);
      break;
    case 'D':
      break;
    case 'X':
      delete(sockfd, response);
      break;
    case 'V':
      display_commands();
      break;
    case 'Q':
      printf("%s\n", response);
      break;
  }  

  if(command == 'Q' && strcmp(response, "Disconnecting...") == 0){
    free(response);
    printf("Thank you for using BitDrive!\n");
    return false;
  }

  free(response);
  return true;
}

char get_command(){
  char command;
  printf("> ");
  scanf("%s", &command);
  return command;
}

char *get_input(){
  printf("> ");
  char *input = malloc(sizeof(char) * 256);
  scanf("%s", input);
  return input;
}

void error_occurred(const char *msg){
  perror(msg);
  exit(0);
}

void run_bitdrive(int sockfd){
  bool is_running = true;
  char command;
  display_commands();

  while(is_running){
    printf("\nWhat would you like to do?\n");
    command = get_command();
    is_running = send_command(command, sockfd);
  }    

  return;
}

void delete(int sockfd, char *response){
  if(strcmp(response, "ready_delete") == 0){
    printf("Which file would you like to delete?\n");
    char *file = get_input();
    send_request(sockfd, file);
    free(file);

    bzero(response, 256);
    recv_response(sockfd, response);
    if(strcmp(response, "delete_success") == 0){
      printf("File deleted successfully!\n");
    }

    else{
      printf("There was an error deleting the file. Please try again.\n");
    }
  }
}

bool send_file(int sockfd, char *filename){
  FILE *file;
  file = fopen(filename, "rb");
  
  int bytes_read = 0;
  char *buffer = malloc(sizeof(char) * 256);
  bzero(buffer, 256);

  if(file == NULL){
    error_occurred("Error opening file.\n");
  }

  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  sprintf(buffer, "%d", size);
  write(sockfd, buffer, 256);
  fseek(file, 0L, SEEK_SET);
  while(true){
    bzero(buffer, 256);
    bytes_read = fread(buffer, 1, 256, file);

    if(bytes_read > 0){
      write(sockfd, buffer, bytes_read);
    }

    if(bytes_read < 256){
      if(feof(file)){
        printf("Upload done!\n");
        return true;
      }

      if(ferror(file)){
        printf("Error uploading file.\n");
        return false;
      }
    }
  }

  free(buffer);
  return false;
}

bool recv_file(int sockfd, char *filename){
  printf("Filename: %s\n", filename);
  FILE *file;
  file = fopen(filename, "w+");

  printf("File opened.\n");
  
  int bytes_received = 0;
  char *buffer;
  buffer = malloc(sizeof(char) * 256);
  bzero(buffer, 256);

  if(file == NULL){
    error_occurred("Error opening file.\n");
  }

  read(sockfd, buffer, 256);
  int size = atoi(buffer);
  int sum_bytes_received = 0;
  int remaining_bytes = 0;

  while(sum_bytes_received < size) {
    bzero(buffer, 256);
    sum_bytes_received += bytes_received;
    remaining_bytes = size - sum_bytes_received;

    if(remaining_bytes < 256){
      bytes_received = read(sockfd, buffer, remaining_bytes);
    }

    else {      
      bytes_received = read(sockfd, buffer, 256);
    }

    if(bytes_received > 0){
      fwrite(buffer, 1, bytes_received, file);
    }
  
    if(bytes_received < 0){
      printf("Error reading file.");
      break;
    }

    if(bytes_received == 0){
      break;
    }
  }

  int status = fclose(file);
  if(status == 0) {
    printf("File received!\n");
    return true;
  }

  else {
    printf("ERROR: File not closed.\n");
  }

  return false;
}

void upload(int sockfd, char *response){
  if(assert_equals(response, "ready_upload")){
    printf("What file do you want to upload?\n");
    char *filename = get_input();
    send_request(sockfd, filename);

    bzero(response, 256);
    recv_response(sockfd, response);
    if(assert_equals(response, "ready_filename")){
      if(send_file(sockfd, filename) == false){
        printf("ERROR: File not sent.\n");
      }
    }

    else {
      printf("Server not yet ready to receive file. Try again.\n");
    }

    free(filename);
  }

  else {
    printf("Server is not yet ready to receive filename. Try again.\n");
  }
}

void test_commands(char command, int sockfd){
  char *response;
  response = process_command(command, sockfd);

  switch(command){
    case 'L':
      printf("> L\n");
      printf("Here are the list of files:\n");
      printf("----------------------------------------------\n"); 
      printf("%s", response);
      printf("----------------------------------------------\n");
      test_list(response);
      break;
    case 'U':
      printf("> U\n");
      test_upload(sockfd, response);
      break;
    case 'D':
      printf("> D\n");
      test_download(sockfd, response);
      break;
    case 'X':
      printf("> X\n");
      test_delete(sockfd, response);
      break;
    case 'Q':
      printf("> Q\n");
      printf("%s\n", response);
      test_quit(response);
      break;
    default:
      break;
  }  

  free(response);
}

void test_upload(int sockfd, char *response){
  if(assert_equals(response, "ready_upload")){
    printf("What file do you want to upload?\n");
    printf("> lorem.txt\n");
    send_request(sockfd, "lorem.txt");

    char path[] = "client_files/";
    strcat(path, "lorem.txt");

    bzero(response, 256);
    recv_response(sockfd, response);
    if(assert_equals(response, "ready_filename")){
      if(send_file(sockfd, path) == true){
        printf("TEST: upload()\t PASS\n\n");
      }

      else{
        printf("TEST: upload()\t FAIL\n\n");        
      }
    }

    else {
      printf("Server not yet ready to receive file. Try again.\n");
    }
  }

  else {
    printf("Server is not yet ready to receive filename. Try again.\n");
  }
}

void test_download(int sockfd, char *response){  
  char path[] = "client_files/";
  if(assert_equals(response, "ready_download")){
    printf("What file do you want to download?\n");
    printf("> readme.txt\n");
    send_request(sockfd, "readme.txt");

    strcat(path, "readme.txt");
    printf("New filename: %s\n", path);

    bzero(response, 256);
    recv_response(sockfd, response);
    if(assert_equals(response, "ready_to_send")){
      printf("Downloading file...\n");
      if(recv_file(sockfd, path) == true){
        printf("TEST: download()\t PASS\n\n");
      }

      else{
        printf("TEST: download()\t FAIL\n\n");        
      }
    }

    else {
      printf("Server not yet ready. Try again.\n");
    }
  }

  else {
    printf("Server is not yet ready. Try again.\n");
  }
}

void test_delete(int sockfd, char *response){
  if(assert_equals(response, "ready_delete")){
    printf("Which file would you like to delete?\n");
    printf("> lorem.txt\n");
    send_request(sockfd, "lorem.txt");

    bzero(response, 256);
    recv_response(sockfd, response);

    if(assert_equals(response, "delete_success")){
      printf("File deleted successfully!\n");
      bzero(response, 256);
      char *list_response = process_command('L', sockfd);
      printf("Files remaining:\n%s", list_response);

      if(assert_equals(list_response, "readme.txt (44 bytes)\n")){
        printf("TEST: delete()\t PASS\n\n");      
      }

      else{
        printf("TEST: delete()\t FAIL\t File not deleted (error: remaining files not correct).\n\n");
      }

      free(list_response);
    }

    else{
      printf("TEST: delete()\t FAIL\t File not deleted.\n\n");      
    }  
  }

  else{
    printf("TEST: delete()\t FAIL\t Server not ready.\n\n");
  }
}

void test_list(char *response){
  if(assert_equals(response, "readme.txt (44 bytes)\nlorem.txt (244 bytes)\n")){
    printf("TEST: list()\t PASS\n\n");
  }

  else{
    printf("TEST: list()\t FAIL\n\n");
  }
}

void test_quit(char *response){
  if(assert_equals(response, "Disconnecting...")){
    printf("Thank you for using BitDrive!\n");
    printf("TEST: quit()\t PASS\n\n");
  }

  else{
    printf("TEST: quit()\t FAIL\n\n");        
  }
}

bool assert_equals(char *actual, char *expected){
  if(strcmp(actual, expected) == 0){
    return true;
  }

  return false;
}

void test(int sockfd){
  int i;
  char commands[5] = {'U', 'L', 'X', 'D', 'Q'};

  for(i = 0; i < 5; i++){
    test_commands(commands[i], sockfd);
    // sleep(5);
  }

  return;
}
