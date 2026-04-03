// Beispiel-Server
#include "poller.h"

#include <array>
#include <iostream>
#include <string>
#include <unordered_set>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif


namespace
{

  void bind_and_listen(int fd, uint16_t port, uint32_t ip)
  {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
    {
      throw std::runtime_error("bind failed");
    }

    if (listen(fd, SOMAXCONN) != 0)
    {
      throw std::runtime_error("listen failed");
    }
  }

  void send_response(int client_fd)
  {
    const std::string body = "Miau Miauu Miauuuu!!!!!\n";
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;

    const char *data = response.data();
    int remaining = static_cast<int>(response.size());

    while (remaining > 0)
    {
      int written = event::send_data(client_fd, data, remaining);
      if (written <= 0)
      {
        break;
      }
      data += written;
      remaining -= written;
    }
  }

}


/* class WebServer {
    ConfigParser config_;  // ← Load once at startup
    
    void start() {
        // 1. AT STARTUP: Create listen sockets from config
        const auto& servers = config_.getServers();
        for (const auto& server : servers) {
            int fd = create_listen_socket(server.port);
            listen_fds_.push_back(fd);
        }
    }
    
    void handle_client_read(int fd) {
        // 2. DURING REQUEST: Check body size limit
        if (conn.parser.body.size() > get_max_body_size()) {
            send_error(413);
        }
    }
    
    void check_timeouts() {
        // 3. TIMEOUT CHECKING: Use config timeout values
        int timeout = config_.getGlobalConfig().timeout;
    }
}; */


    // Create and setup listening socket
/*     int listen_fd = event::create_socket();
    event::set_socket_reuse(listen_fd);
    bind_and_listen(listen_fd, conf.getServers()[0].listen_port[0].port);
    event::set_non_blocking(listen_fd);

    auto poller = event::make_poller();
    poller->add(listen_fd, true, false); */
int create_and_listen(const ListenPort& lp, event::EventPoller& poller)
{
    int fd = event::create_socket();
    event::set_socket_reuse(fd);
    bind_and_listen(fd, lp.port, lp.ip);
    event::set_non_blocking(fd);
    poller.add(fd, true, false);
    return fd;
}

// ADDED: get config port & ip & iterate multiple servers and ports
// need to add ip for bind_and_listen
//
int main(int argc, char *argv[])
{
  try
  {
    ConfigParser conf = (argc > 1) ? ConfigParser(argv[1]) : ConfigParser();
    conf.parse();

    std::vector<int> listen_fds;
    auto poller = event::make_poller();
  for (const auto &server : conf.getServers())
  {
    for (const auto &lp : server.listen_port)
    {
        listen_fds.push_back(create_and_listen(lp, *poller));
        std::cout << "Listening on http://"
                  << inet_ntoa(*(struct in_addr *)&lp.ip)
                  << ":" << lp.port << "\n";
    }
}

    std::unordered_set<int> clients;
    std::unordered_set<int> listen_fd_set(listen_fds.begin(), listen_fds.end());
    std::unordered_map<int, HttpRequest> requests;

    while (true)
    {
      for (const auto &event : poller->wait(1000))
      {
        /* if (event.fd == listen_fd && event.readable) */
        if (listen_fd_set.count(event.fd) && event.readable)
        {
          // Accept all pending connections
          while (true)
          {
            int client_fd = event::accept_connection(event.fd);
            if (client_fd == -1)
            {
              break; // No more connections
            }
            event::set_non_blocking(client_fd);
            poller->add(client_fd, true, false);
            clients.insert(client_fd);
            requests.emplace(client_fd, HttpRequest());
          }
        }
        else if (event.readable)
        {
          // Read from client
          std::array<char, 1024> buffer{};
          int bytes = event::receive_data(event.fd, buffer.data(), static_cast<int>(buffer.size()));

          if (bytes <= 0)
          {
            // Connection closed or error
            poller->remove(event.fd);
            clients.erase(event.fd);
            event::close_socket(event.fd);
            continue;
          }

      HttpRequest& req = requests[event.fd];
      ParseState state;
      //refactor to only return state no error throwing?
      try
      {
        state = req.parseRequest(buffer.data(), bytes);
      }
      catch (std::exception &e)
      {
        state = ERROR;
      }

      if (state == COMPLETE)
      {
        // handle_request(req); actually dont want to always close
/*         send_response(event.fd);
        poller->remove(event.fd);
        clients.erase(event.fd);
        requests.erase(event.fd);
        event::close_socket(event.fd); */
      }
    else if (state == ERROR)
    {
        // send error response later
        poller->remove(event.fd);
        clients.erase(event.fd);
        requests.erase(event.fd);
        event::close_socket(event.fd);
    }
        }
      }
    }
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Server error: " << ex.what() << "\n";
    return 1;
  }
}
