#ifndef CHAT_H_
#define CHAT_H_

#include <string>

int get_id();

void register_connection(struct conn_info conn_info);

struct conn_info make_conn_info(int socket, int port, struct sockaddr* dest_addr);

bool running_check();

void listen_messages(int id);

void send_message(int id, char message[100]);

void print_listen_failure_msg(int port); 

void listen_new_connections(int port);

void connect(std::string dest, int port); 

void myip(); 

void help(); 

void list();

void terminate(int id);

void handle_cin(int port); 

#endif
