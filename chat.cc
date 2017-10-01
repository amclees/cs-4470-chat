#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <iostream>
#include <string>
#include <cstring>

void print_listen_failure_msg(int port) {
  std::cout << "Unable to begin listening on port " << port << ". No connections will be accepted, but you can still initiate connections from this computer.\n";
}

void listen_new_connections(int port) {
  int status;
  struct addrinfo hints;
  struct addrinfo* servinfo;
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

  // Make new connections
  // Make new connections' threads

  freeaddrinfo(servinfo);
}

void listen_messages(/*socket*/) {

}

void connect(std::string dest, int port) {

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
  std::thread new_connections(listen_new_connections, port);

  handle_cin();

  return 0;
}