#ifndef CHAT_H_
#define CHAT_H_

#include <string>
#include <map>
#include <list>
#include <arpa/inet.h>

struct conn_info {
  int id;
  int socket;
  int port;
  bool terminate;
  char ip_str[INET6_ADDRSTRLEN];
  char port_str[6];
};

struct conn_ledger {
  std::list<int>* list;
  std::map<int, struct conn_info>* map;
};

int get_port(struct sockaddr *sa);

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
