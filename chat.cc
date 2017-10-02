#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <string>
#include <cstring>
#include <list>
#include <map>
#include <mutex>

std::mutex conn_info_mutex;
int id = 0;
bool global_exit = false;

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

int get_id() {
  conn_info_mutex.lock();
  int new_id = id++;
  conn_info_mutex.unlock();
  return new_id;
}

void register_connection(struct conn_ledger* ledger, struct conn_info conn_info) {
  std::cerr << "Made connection with\n  socket descriptor " << conn_info.socket << "\n  " << conn_info.ip_str << ":" << conn_info.port_str << "\n";
  ledger->list->push_back(conn_info.id);
  ledger->map->insert(std::pair<int, struct conn_info>(conn_info.id, conn_info));
}

struct conn_info make_conn_info(int socket, int port, struct sockaddr* dest_addr) {
  struct conn_info conn_info;
  conn_info.id = get_id();
  conn_info.socket = socket;
  conn_info.port = port;
  conn_info.terminate = false;
  
  switch (dest_addr->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET, &(((struct sockaddr_in*)dest_addr)->sin_addr), conn_info.ip_str, INET_ADDRSTRLEN);
      break;
    case AF_INET6:
      inet_ntop(AF_INET6, &(((struct sockaddr_in6*)dest_addr)->sin6_addr), conn_info.ip_str, INET6_ADDRSTRLEN);
      break;
    default:
      std::cerr << "Invalid address family in dest_addr struct when making connection info.\n";
      break;
  }

  sprintf(conn_info.port_str, "%d", port);

  return conn_info;
}

// Checks the termination variable and returns false if the thread should clean up and check in
bool running_check() {
  return !global_exit;
}

void print_listen_failure_msg(int port) {
  std::cout << "Unable to begin listening on port " << port << ". No connections will be accepted, but you can still initiate connections from this computer.\n";
}

void listen_new_connections(struct conn_ledger* ledger, int port) {
  int status;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct sockaddr_storage dest_addr;
  socklen_t addr_size = sizeof(dest_addr);
  char port_str[6];
  sprintf(port_str, "%d", port);

  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
    std::cerr << "getaddrinfo error: %s" << gai_strerror(status) << "\n";
    print_listen_failure_msg(port);
    std::terminate();
  }

  int new_conn_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (new_conn_socket == -1) {
    std::cerr << "Unable to get socket descriptor to listen for connections.\n";
    print_listen_failure_msg(port);
    std::terminate();
  }

  int bind_status = bind(new_conn_socket, servinfo->ai_addr, servinfo->ai_addrlen);
  if (bind_status == -1) {
    std::cerr << "Unable to bind to port " << port << ".\n";
    print_listen_failure_msg(port);
    std::terminate();
  }

  while (running_check()) {
    int listen_status = listen(new_conn_socket, 5);
    if (listen_status == -1) {
      std::cerr << "Unable to listen on port " << port << ".\n";
      print_listen_failure_msg(port);
      std::terminate();
    }

    int accepted_socket = accept(new_conn_socket, (struct sockaddr*)&dest_addr, &addr_size);
    if (accepted_socket == -1) {
      std::cerr << "Unable to accept new connection.\n";
      std::cout << "Failed to accept an incoming connection; continuing to listen for new connections.\n";
      continue;
    }

    register_connection(ledger, make_conn_info(accepted_socket, port, (struct sockaddr*)&dest_addr));
  }

  freeaddrinfo(servinfo);
}

void listen_messages(struct conn_ledger* ledger, int id, int socket) {
  char message_buf[100];
  while(running_check() && !ledger->map->at(id).terminate) {
    int recv_status = recv(socket, message_buf, 100, 0);

    switch (recv_status) {
      case 0:
        std::cout << "Connection #" << id << " at " << ledger->map->at(id).ip_str << " port " << ledger->map->at(id).port_str << "\n";
        break;
      case -1:
        std::cout << "Error receiving message from connection #" << id << "\n";
        break;
    }

    std::cout << "Message received from " << ledger->map->at(id).ip_str << "\n";
    std::cout << "Sender's Port: " << ledger->map->at(id).port_str << "\n";
    std::cout << "Message: \"" << message_buf << "\"\n";
  }
  close(socket);
}

void connect(struct conn_ledger* ledger, std::string dest, int port) {
  int status;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct sockaddr_storage dest_addr;
  socklen_t addr_size = sizeof(dest_addr);
  char port_str[6];
  sprintf(port_str, "%d", port);

  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
    std::cerr << "getaddrinfo error: %s" << gai_strerror(status) << "\n";
    return;
  }

  int new_conn_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (new_conn_socket == -1) {
    std::cerr << "Unable to get socket descriptor to connect";
    // TODO: Error message
    return;
  }

  //register_connection(ledger, make_conn_info(accepted_socket, port, (struct sockaddr*)&dest_addr));

  //std::thread incoming_messages(ledger, listen_messages, ledger, id, port);

  freeaddrinfo(servinfo);
}

void handle_cin() {

}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Wrong number of arguments. Call as `chat <port>`\n";
    return 1;
  }
  std::string port_string = argv[1];
  int port = -1;
  try {
    port = std::stoi(port_string);
  } catch (std::invalid_argument exception) {
    port = -1;
  } catch (std::out_of_range) {
    port = -1;
  }

  if (port < 1 || port > 65535) {
    std::cout << "The port must be a number between 1 and 65535\n";
    return 1;
  }

  std::cout << "Initializing chat on port " << port << "\n";
  struct conn_ledger ledger;
  std::list<int> ledger_list;
  std::map<int, struct conn_info> ledger_map;
  ledger.list = &ledger_list;
  ledger.map = &ledger_map;

  std::thread new_connections(&ledger, listen_new_connections, port);

  handle_cin();

  return 0;
}
