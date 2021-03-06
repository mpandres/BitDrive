#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

const int NUM_CLIENTS = 2;
int client_number;
struct File{
  char *filename;
  float size;
  struct File *next;
};

char *read_request(int sockfd, char *buffer);
void write_response(int clientfd, char *response);
void *recv_file(int clientfd, char *filename);
void *send_file(int clientfd, char *filename);
void *communicate(void *newsockfd);
void start_server(int port);
void free_list(struct File *head);
struct File* create_list();
void list(struct File *root, int clientfd);
void upload(int clientfd, char *request);
void download(int clientfd, char *request);
void delete(int clientfd, char *request);
void quit(int clientfd);
void invalid_input(int clientfd);
void process_request(char *request, int sockfd);
void display_welcome();
void set_sockaddr(struct sockaddr_in *socket_addr, int port);
void error_occurred(const char *msg);

int main(int argc, char* argv[]) {
  if(argc < 2) {
    error_occurred("No port was provided");
  }

  display_welcome();
  start_server(atoi(argv[1]));
  return 0;
}

void display_welcome(){
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  printf("Welcome to BitDrive Server! \n");
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

void start_server(int port){
  /* Initialization of Variables */
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len;
  int sockfd, newsockfd, status;
  pthread_t thread_id[NUM_CLIENTS];

  /* Initial Values */
  set_sockaddr(&server_addr, htons(port));
  client_len = sizeof(client_addr);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    error_occurred("ERROR opening socket");
  }

  status = bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
  if(status < 0) {
    error_occurred("ERROR on binding");
  }

  listen(sockfd, 5);
  printf("Server has started.\n");
  printf("Now listening to port: %d \n", port);

  /* Waiting for client to connect */
  client_number = 0;
  while(true) {
    if(client_number < NUM_CLIENTS){
      newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_len);
      if (newsockfd < 0) {
        error_occurred("ERROR on accept");
      }

      pthread_create(&thread_id[client_number],
                    NULL, communicate, (void*) &newsockfd);

      client_number++;
    }
  }
}

void process_request(char *request, int clientfd){
  printf("Client %d: %s\n", clientfd, request);
  if(strcmp(request, "LIST") == 0){
    list(create_list(), clientfd);
  }

  else if(strcmp(request, "UPLOAD") == 0){
    upload(clientfd, request);
  }

  else if(strcmp(request, "DOWNLOAD") == 0){
    download(clientfd, request);
  }

  else if(strcmp(request, "DELETE") == 0){
    delete(clientfd, request);
  }

  else if(strcmp(request, "QUIT") == 0){
    quit(clientfd);
  }

  else {
    invalid_input(clientfd);
  }
}

void set_sockaddr(struct sockaddr_in *socket_addr, int port){
  bzero((char *) socket_addr, sizeof(*socket_addr));
  socket_addr->sin_family = AF_INET;
  socket_addr->sin_addr.s_addr = INADDR_ANY;
  socket_addr->sin_port = port;
}

void error_occurred(const char *msg){
  perror(msg);
  exit(1);
}

void *recv_file(int clientfd, char *filename){
  char *path = malloc(sizeof(char) * 13 + sizeof(filename));
  strcpy(path, "server_files/");
  FILE *file;
  file = fopen(strcat(path, filename), "w+");

  int bytes_received = 0;
  char *buffer;
  buffer = malloc(sizeof(char) * 256);
  bzero(buffer, 256);

  if(file == NULL){
    error_occurred("Error opening file.\n");
  }

  read(clientfd, buffer, 256);
  int size = atoi(buffer);
  int sum_bytes_received = 0;
  int remaining_bytes = 0;

  while(sum_bytes_received < size) {
    bzero(buffer, 256);
    sum_bytes_received += bytes_received;
    remaining_bytes = size - sum_bytes_received;

    if(remaining_bytes < 256){
      bytes_received = read(clientfd, buffer, remaining_bytes);
    }

    else {
      bytes_received = read(clientfd, buffer, 256);
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
  }
  else {
    printf("ERROR: File not closed.\n");
  }

  free(buffer);
  free(path);
  return 0;
}

void *send_file(int clientfd, char *filename){
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
  write(clientfd, buffer, 256);
  fseek(file, 0L, SEEK_SET);

  while(true){
    bzero(buffer, 256);
    bytes_read = fread(buffer, 1, 256, file);

    if(bytes_read > 0){
      write(clientfd, buffer, bytes_read);
    }

    if(bytes_read < 256){
      if(feof(file)){
        printf("Download done!\n");
      }

      if(ferror(file)){
        printf("Error uploading file.\n");
      }

      break;
    }
  }

  fclose(file);
  free(buffer);
  return 0;
}

void write_response(int clientfd, char *response){
  int status = write(clientfd, response, strlen(response));
  if(status < 0) {
    error_occurred("ERROR writing to socket");
  }
}

char *read_request(int sockfd, char *buffer){
  bzero(buffer, 1024);
  int status = read(sockfd, buffer, 1024);
  if(status < 0) {
    error_occurred("ERROR reading from socket");
  }

  return buffer;
}

void *communicate(void *newsockfd){
  int sockfd = *((int*)newsockfd);
  printf("Connected to client %d...\n", sockfd);
  bool server_run = true;
  char *buffer = malloc(sizeof(char) * 1024);
  bzero(buffer, 1024);

  while(server_run) {
    buffer = read_request(sockfd, buffer);
    process_request(buffer, sockfd);
    if(strcmp(buffer, "QUIT") == 0){
      server_run = false;
    }
  }

  free(buffer);
  close(sockfd);
  return 0;
}

void upload(int clientfd, char *request){
  char *response = "ready_upload";
  printf("Client %d: %s\n", clientfd, response);
  write_response(clientfd, response);

  // get filename
  request = read_request(clientfd, request);
  if(strcmp(request, "filename_error") == 0){
    printf("File does not exist. Aborting upload.\n");
    return;
  }

  printf("Client %d: %s\n", clientfd, request);

  // ready to receive
  response = "ready_filename";
  printf("Client %d: %s\n", clientfd, response);
  write_response(clientfd, response);

  // receive the file
  recv_file(clientfd, request);
}

void download(int clientfd, char *request){
  printf("Client %d: %s\n", clientfd, "ready_download");
  write_response(clientfd, "ready_download");

  // get filename
  request = read_request(clientfd, request);
  char *path = malloc(sizeof(char) * 13 + sizeof(request));
  strcpy(path, "server_files/");
  strcat(path, request);

  FILE *file = fopen(path, "r");
  if(file == NULL){
    printf("File does not exist. Aborting download.\n");
    write_response(clientfd, "filename_error");
    return;
  }

  fclose(file);
  printf("Client %d: %s\n", clientfd, request);

  // ready to send
  write_response(clientfd, "ready_to_send");
  printf("Client %d: %s\n", clientfd, "ready_to_send");

  request = read_request(clientfd, request);
  printf("Client %d: %s\n", clientfd, request);

  if(strcmp(request, "ready_to_receive") == 0){
    send_file(clientfd, path);
  }

  else {
    printf("Client not ready. Aborting download.\n");
  }

  free(path);
}

void delete(int clientfd, char *request){
  char *response = "ready_delete";
  char *path = malloc(sizeof(char) * 13);
  strcpy(path, "server_files/");
  write_response(clientfd, response);

  // get file to delete
  request = read_request(clientfd, request);
  printf("Client %d: %s\n", clientfd, request);

  // delete file
  char *file_path = malloc(strlen(path) + strlen(request) + 1);
  strcpy(file_path, path);
  strcat(file_path, request);

  if(remove(file_path) == 0){
    response = "delete_success";
  }

  else {
    response = "delete_error";
  }

  free(file_path);
  free(path);
  write_response(clientfd, response);
}

void quit(int clientfd){
  char *response = "Disconnecting...";
  write_response(clientfd, response);
  client_number--;
}

void invalid_input(int clientfd){
  char *response = "Invalid input. Please try again.";
  write_response(clientfd, response);
}

void free_list(struct File* head){
  struct File *tmp;
  int counter = 0;
  while (head != NULL) {
    tmp = head;
    counter++;

    head = head->next;
    free(tmp->filename);
    free(tmp);
  }

  free(head);
}

struct File* create_list(){
  struct stat file_stats;
  struct File *root;
  char *directory = "server_files";
  char *path = "server_files/";

  root = (struct File *)malloc(sizeof(struct File));
  root->next = NULL;
  root->filename = NULL;

  struct File *current;
  current = root;

  DIR *dir;
  struct dirent *ent;
  dir = opendir(directory);

  if(dir){
    while((ent = readdir(dir)) != NULL){
      if(ent->d_type == 8){
        if(current->filename != NULL){
          struct File *file;
          file = (struct File *)malloc(sizeof(struct File));
          current->next = file;
          current = file;
          current->next = NULL;
        }

        char *file_path = malloc(strlen(path) + strlen(ent->d_name) + 1);
        char *filename = malloc(strlen(ent->d_name) + 1);
        strcpy(filename, ent->d_name);
        strcpy(file_path, path);
        strcat(file_path, ent->d_name);
        stat(file_path, &file_stats);

        current->filename = filename;
        current->size = (float)(file_stats.st_size/1000.0);
        free(file_path);
      }
    }

    closedir(dir);
  }

  else {
    printf("There are no files available for download.\n");
  }

  free(ent);
  return root;
}

void list(struct File *root, int clientfd){
  struct File *current;
  current = root;
  float size = 0;
  int file_counter = 0;

  // check if list of files is empty first
  if(current->filename == NULL){
    printf("No files found.\n");
    write_response(clientfd, "0");
    free_list(root);
    return;
  }

  // get size for malloc
  while (current != NULL){
    size += sizeof(*current);
    file_counter++;
    current = current->next;
  }

  char *list_string = malloc(size + (20*file_counter) + file_counter);
  char *buffer = malloc(sizeof(char) * 1024);
  sprintf(buffer, "%d", file_counter);
  write_response(clientfd, buffer);
  printf("Files found: %s\n", buffer);

  bzero(buffer, 1024);
  read(clientfd, buffer, 1024);
  printf("%s\n", buffer);

  if(strcmp(buffer, "file_count_received") != 0){
    printf("Client not ready. Aborting command.\n");
    free(buffer);
    free(list_string);
  }

  free(buffer);

  strcpy(list_string, "\0");
  current = root;
  while (current != NULL){
    //convert current->size to string
    char *file_size = malloc(sizeof(float) * current->size);
    sprintf(file_size, "%.1f", current->size);

    strcat(list_string, current->filename);
    strcat(list_string, " (");
    strcat(list_string, file_size);
    strcat(list_string, " kb)\n");
    current = current->next;
    free(file_size);
  }

  free_list(root);

  printf("Files found:\n%s\n", list_string);
  write_response(clientfd, list_string);
  free(list_string);
}
