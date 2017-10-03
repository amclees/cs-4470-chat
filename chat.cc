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
#include <sstream>
#include <vector>
#include <iterator>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>

#include <chat.h>

std::mutex conn_info_mutex;
int id_pool = 0;
bool global_exit = false;

struct conn_ledger ledger;
std::list<int> ledger_list;
std::map<int, struct conn_info> ledger_map;

// Returns a new unique ID for each connection
int get_id() {
  conn_info_mutex.lock();
  int new_id = id_pool++;
  conn_info_mutex.unlock();
  return new_id;
}

void register_connection(struct conn_info conn_info) {
  std::cerr << "Made connection with\n  socket descriptor " << conn_info.socket << "\n  " << conn_info.ip_str << " port " << conn_info.port_str << std::endl;
  ledger.list->push_back(conn_info.id);
  ledger.map->insert(std::pair<int, struct conn_info>(conn_info.id, conn_info));
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
      std::cerr << "Invalid address family: \"" << dest_addr->sa_family << "\" in dest_addr struct when making connection info." << std::endl;
      break;
  }

  sprintf(conn_info.port_str, "%d", port);

  return conn_info;
}

// Checks the termination variable and returns false if the thread should clean up and check in
bool running_check() {
  return !global_exit;
}

void listen_messages(int id) {
  char message_buf[100];
  bool should_break = false;
  while(running_check() && !ledger.map->at(id).terminate) {
    int recv_status = recv(ledger.map->at(id).socket, message_buf, 100, 0);
    switch (recv_status) {
      case 0:
        std::cout << "Connection closed: #" << id << " at " << ledger.map->at(id).ip_str << " port " << ledger.map->at(id).port_str << std::endl;
        should_break = true;
        break;
      case -1:
        if (errno == 107) {
          std::cout << "Failed to connect to connection #" << id << std::endl;
          should_break = true;
          break;
        }
        std::cout << "Error " << errno << " receiving message from connection #" << id << std::endl;
        should_break = true;
        break;
    }

    if (should_break) {
      break;
    }

    std::cout << "Message received from " << ledger.map->at(id).ip_str << std::endl;
    std::cout << "Sender's Port: " << ledger.map->at(id).port_str << std::endl;
    std::cout << "Message: " << message_buf << std::endl;
  }
  close(ledger.map->at(id).socket);
  std::cout << std::endl;
  std::cout << "Connection #" << id << " terminated" << std::endl;
}

void send_message(int id, char message[100]) {
  int socket = ledger.map->at(id).socket;
  int send_status = send(socket, message, 100, 0);
  switch (send_status) {
    case -1:
      std::cout << "Error sending message to connection #" << id << std::endl;
    default:
      std::cout << "Message sent to connection #" << id << std::endl;
      break;
  }
}

void print_listen_failure_msg(int port) {
  std::cout << "Unable to begin listening on port " << port << ". No connections will be accepted, but you can still initiate connections from this computer." << std::endl;
}

void listen_new_connections(int port) {
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
    std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
    print_listen_failure_msg(port);
    std::terminate();
  }

  int new_conn_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (new_conn_socket == -1) {
    std::cerr << "Unable to get socket descriptor to listen for connections." << std::endl;
    print_listen_failure_msg(port);
    std::terminate();
  }

  int bind_status = bind(new_conn_socket, servinfo->ai_addr, servinfo->ai_addrlen);
  if (bind_status == -1) {
    std::cerr << "Unable to bind to port " << port << std::endl;
    print_listen_failure_msg(port);
    std::terminate();
  }

  while (running_check()) {
    int listen_status = listen(new_conn_socket, 5);
    if (listen_status == -1) {
      std::cerr << "Unable to listen on port " << port << std::endl;
      print_listen_failure_msg(port);
      std::terminate();
    }

    int accepted_socket = accept(new_conn_socket, (struct sockaddr*)&dest_addr, &addr_size);
    if (accepted_socket == -1) {
      std::cerr << "Unable to accept new connection." << std::endl;
      std::cout << "Failed to accept an incoming connection; continuing to listen for new connections." << std::endl;
      continue;
    }

    struct conn_info conn_info = make_conn_info(accepted_socket, port, (struct sockaddr*)&dest_addr);
    register_connection(conn_info);
    std::thread listener_thread(listen_messages, conn_info.id);
    listener_thread.detach();
  }

  freeaddrinfo(servinfo);
}

void connect(std::string dest, int port) {
  int status;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  char port_str[6];
  sprintf(port_str, "%d", port);
  const char* dest_str = dest.c_str();

  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(dest_str, port_str, &hints, &servinfo)) != 0) {
    std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n" << std::endl;
    std::cout << "Failed to resolve location; please check that you entered a valid host" << std::endl;
    return;
  }

  int socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (socket_fd == -1) {
    std::cerr << "Unable to get socket descriptor to connect" << std::endl;
    std::cout << "Failed to connect to the location, check that it is available" << std::endl;
    return;
  }

  connect(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen);

  struct conn_info conn_info = make_conn_info(socket_fd, port, servinfo->ai_addr);

  register_connection(conn_info);

  std::thread incoming_messages(listen_messages, conn_info.id);
  incoming_messages.detach();

  freeaddrinfo(servinfo);
}

void myip() {
  struct ifaddrs *myaddrs, *ips;
  void *in_addr;
  char buf[64];
  
  if (getifaddrs(&myaddrs) != 0){
    perror("getifaddrs");
    exit(1);
  }

  for (ips = myaddrs; ips != NULL; ips =ips->ifa_next){
    if (ips->ifa_addr == NULL) {
      continue;
    }
    if (!(ips->ifa_flags & IFF_UP)) {
      continue;
    }

    switch (ips->ifa_addr->sa_family) {
      case AF_INET:
        {
          struct sockaddr_in *s4 = (struct sockaddr_in *)ips->ifa_addr;
          in_addr = &s4->sin_addr;
          break;
        }
      default:
        continue;
    }

    if (!inet_ntop(ips->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) {
      printf("%s: inet_ntop failed!\n", ips->ifa_name);
    }
  }

  freeifaddrs(myaddrs);
  printf("%s\n", buf);
}

void help() {
  std::cout << "myip : Displays host ip address" << std::endl;
  std::cout << "myport : Displays port currently listening for incoming connections" << std::endl;
  std::cout << "connect :<destination id> <port no> : Attempts to connect to another computer" << std::endl;
  std::cout << "list : Prints a list of all saved connections" << std::endl;
  std::cout << "terminate connection id> : Closes the selected connections" << std::endl;
  std::cout << "send <connection id> <message> : Sends a message to the selected connection" << std::endl;
  std::cout << "exit : Terinates all existing connections  and terminates the program" << std::endl;
}

void list() {
  if (ledger.list->empty()) {
    std::cout << "No connections" << std::endl;
    return;
  }
  std::cout << "Id: IP address         Port No." << std::endl;
  for (int item : *(ledger.list)) {
    std::cout << (*(ledger.map))[item].id <<": " << (*(ledger.map))[item].ip_str << "       "    <<  (*(ledger.map))[item].port << std::endl;
  }
}

void terminate(int id) {
  if (0 != (*(ledger.map)).count(id)) {
    (*(ledger.map))[id].terminate = true;
    (*(ledger.list)).remove(id);
    (*(ledger.map)).erase(id);
    std::cout << "Terminated connection" << std::endl;
  } else {
    std::cout << "Connection does not exist" << std::endl;
  }
}

void handle_cin(int port) {
  std::string input;    
  while (true) {
    std::cout << "@^@: ";
    std::getline(std::cin, input);
    if (input.length() < 1) {
      continue;
    }
    std::istringstream iss(input);
    std::vector<std::string> results((std::istream_iterator<std::string>(iss)),
        std::istream_iterator<std::string>());

    if (results.size() > 3) {
      for (uint i = 3; i < results.size(); i++) {
        results[2] += " " + results[i];
      }
    }

    if (results[0] == "help") {
      help();
    } else if (results[0] == "myip") {
      myip();    
    } else if (results[0] == "myport") {
      std::cout << "Port: "<< port << std::endl;
    } else if (results.size() == 3 && results[0] == "connect") {
      int port_c = -1;
      try {
        port_c = std::stoi(results[2]);
      } catch (std::invalid_argument exception) {
        port_c = -1;
      } catch (std::out_of_range) {
        port_c = -1;
      }

      std::string local_ip = "127.0.0.1";
      std::string local_name = "localhost";
      if (port == port_c && (results[1].compare(local_ip) == 0 || results[1].compare(local_name) == 0)) {
        std::cout << "No connections can be made to the same instance of this program" << std::endl;
        continue;
      }
  
      connect(results[1], port_c);
    } else if (results[0] == "list"){
      list();
    } else if (results[0] == "terminate" && results.size() == 2) {
      int to_terminate = -1;
      try {
        to_terminate = std::stoi(results[1]);
      } catch (std::invalid_argument exception) {
        to_terminate = -1;
      } catch (std::out_of_range) {
        to_terminate = -1;
      }

      if (ledger.map->count(to_terminate) == 0 || ledger.map->at(to_terminate).terminate) {
        std::cout << "There is no connection with that id" << std::endl;
        continue;
      }

      terminate(to_terminate);  
    } else if (results[0] == "send" && results.size() >= 3) {
      if (results[2].length() > 100) {
        std::cout << "That message is too long." << std::endl;
        continue;
      }

      int dest = -1;
      try {
        dest = std::stoi(results[1]);
      } catch (std::invalid_argument exception) {
        dest = -1;
      } catch (std::out_of_range) {
        dest = -1;
      }

      if (ledger.map->count(dest) == 0 || ledger.map->at(dest).terminate) {
        std::cout << "There is no connection with that id" << std::endl;
        continue;
      }

      char padded[100];
      for (int padding = results[2].length(); padding <= 100; padding++) {
        results[2] += " ";
      }
      strcpy(padded, results[2].c_str());
      send_message(dest, padded);
    } else if (results[0] == "exit") {
      global_exit = true;
      return;
    } else {
      std::cout << "Invalid command: please check that you are using the right arguments." << std::endl;
    }
  }
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
  ledger.list = &ledger_list;
  ledger.map = &ledger_map;

  std::thread new_connections(listen_new_connections, port);
  new_connections.detach();

  handle_cin(port);

  std::cout << "Successfully exited" << std::endl;

  return 0;
}
