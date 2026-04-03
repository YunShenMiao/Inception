#include "../../include/poller.h"

#include <algorithm>
#include <array>
#include <memory>

#ifndef _WIN32
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

namespace event
{
  namespace
  {

    std::string system_message(const char *action)
    {
#ifdef _WIN32
      return std::string(action) + ": " + std::system_category().message(GetLastError());
#else
      return std::string(action) + ": " + std::system_category().message(errno);
#endif
    }

#if defined(__linux__)
  // Linux-Backend: epoll – readiness-Demultiplexing
    uint32_t to_epoll_events(bool readable, bool writable)
    {
      uint32_t events = 0;
      if (readable)
      {
        events |= EPOLLIN;
      }
      if (writable)
      {
        events |= EPOLLOUT;
      }
      return events;
    }
#endif

#ifndef _WIN32
    int socket_pending_error(int fd)
    {
      int socket_error = 0;
      socklen_t len = sizeof(socket_error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &len) != 0)
      {
        return -1;
      }
      return socket_error;
    }
#endif

  }

#if defined(__linux__)

  EpollPoller::EpollPoller()
  {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
      throw std::system_error(errno, std::system_category(), "epoll_create1 failed");
    }
  }

  EpollPoller::~EpollPoller()
  {
    if (epoll_fd_ != -1)
    {
      close(epoll_fd_);
    }
  }

  void EpollPoller::add(int fd, bool readable, bool writable)
  {
    epoll_event event{};
    event.data.fd = fd;
    event.events = to_epoll_events(readable, writable);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1)
    {
      throw std::system_error(errno, std::system_category(), system_message("epoll add"));
    }
  }

  void EpollPoller::update(int fd, bool readable, bool writable)
  {
    epoll_event event{};
    event.data.fd = fd;
    event.events = to_epoll_events(readable, writable);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1)
    {
      // Late MOD calls can race with close/remove paths.
      if (errno == ENOENT)
      {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == 0)
          return;
        if (errno == EEXIST)
          return;
      }
      if (errno == EBADF || errno == ENOENT)
        return;
      throw std::system_error(errno, std::system_category(), system_message("epoll mod"));
    }
  }

  void EpollPoller::remove(int fd)
  {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
      if (errno != ENOENT && errno != EBADF)
      {
        throw std::system_error(errno, std::system_category(), system_message("epoll del"));
      }
    }
  }

  std::vector<PollEvent> EpollPoller::wait(int timeout_ms)
  {
    std::array<epoll_event, 64> events{};
    int count = epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), timeout_ms);
    if (count < 0)
    {
      if (errno == EINTR)
      {
        return {};
      }
      throw std::system_error(errno, std::system_category(), system_message("epoll wait"));
    }

    std::vector<PollEvent> ready;
    ready.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
      const uint32_t event_mask = events[i].events;
      const bool has_error = (event_mask & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0;
      ready.push_back({events[i].data.fd,
                       (event_mask & EPOLLIN) != 0 || has_error,
                       (event_mask & EPOLLOUT) != 0 || has_error});
    }
    return ready;
  }

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

  namespace
  {

    int to_kevent_flags(bool enable)
    {
      return enable ? EV_ADD | EV_ENABLE : EV_DELETE;
    }

  } // namespace

  KqueuePoller::KqueuePoller()
  {
    kqueue_fd_ = kqueue();
    if (kqueue_fd_ == -1)
    {
      throw std::system_error(errno, std::system_category(), "kqueue failed");
    }
  }

  KqueuePoller::~KqueuePoller()
  {
    if (kqueue_fd_ != -1)
    {
      close(kqueue_fd_);
    }
  }

  void KqueuePoller::add(int fd, bool readable, bool writable)
  {
    std::array<kevent, 2> changes{};
    int change_count = 0;
    if (readable)
    {
      EV_SET(&changes[change_count++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    }
    if (writable)
    {
      EV_SET(&changes[change_count++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    }
    if (change_count > 0 && kevent(kqueue_fd_, changes.data(), change_count, nullptr, 0, nullptr) == -1)
    {
      throw std::system_error(errno, std::system_category(), system_message("kqueue add"));
    }
  }

  void KqueuePoller::update(int fd, bool readable, bool writable)
  {
    std::array<kevent, 2> changes{};
    EV_SET(&changes[0], fd, EVFILT_READ, to_kevent_flags(readable), 0, 0, nullptr);
    EV_SET(&changes[1], fd, EVFILT_WRITE, to_kevent_flags(writable), 0, 0, nullptr);
    if (kevent(kqueue_fd_, changes.data(), 2, nullptr, 0, nullptr) == -1)
    {
      throw std::system_error(errno, std::system_category(), system_message("kqueue update"));
    }
  }

  void KqueuePoller::remove(int fd)
  {
    std::array<kevent, 2> changes{};
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    if (kevent(kqueue_fd_, changes.data(), 2, nullptr, 0, nullptr) == -1)
    {
      if (errno != ENOENT)
      {
        throw std::system_error(errno, std::system_category(), system_message("kqueue delete"));
      }
    }
  }

  std::vector<PollEvent> KqueuePoller::wait(int timeout_ms)
  {
    std::array<kevent, 64> events{};
    timespec timeout{};
    timespec *timeout_ptr = nullptr;
    if (timeout_ms >= 0)
    {
      timeout.tv_sec = timeout_ms / 1000;
      timeout.tv_nsec = (timeout_ms % 1000) * 1000000L;
      timeout_ptr = &timeout;
    }
    int count = kevent(kqueue_fd_, nullptr, 0, events.data(), static_cast<int>(events.size()), timeout_ptr);
    if (count < 0)
    {
      if (errno == EINTR)
      {
        return {};
      }
      throw std::system_error(errno, std::system_category(), system_message("kqueue wait"));
    }

    std::vector<PollEvent> ready;
    ready.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
      const bool has_error = (events[i].flags & (EV_EOF | EV_ERROR)) != 0;
      ready.push_back({static_cast<int>(events[i].ident),
                       events[i].filter == EVFILT_READ || has_error,
                       events[i].filter == EVFILT_WRITE || has_error});
    }
    return ready;
  }

#elif defined(_WIN32)
  // Windows-Backend: select
  // Why: IOCP would be more complicated and needs a completely different, asynchronous design 

  void SelectPoller::add(int fd, bool readable, bool writable)
  {
    if (readable)
    {
      readable_fds_.insert(fd);
    }
    if (writable)
    {
      writable_fds_.insert(fd);
    }
  }

  void SelectPoller::update(int fd, bool readable, bool writable)
  {
    if (readable)
    {
      readable_fds_.insert(fd);
    }
    else
    {
      readable_fds_.erase(fd);
    }
    if (writable)
    {
      writable_fds_.insert(fd);
    }
    else
    {
      writable_fds_.erase(fd);
    }
  }

  void SelectPoller::remove(int fd)
  {
    readable_fds_.erase(fd);
    writable_fds_.erase(fd);
  }

  std::vector<PollEvent> SelectPoller::wait(int timeout_ms)
  {
    if (readable_fds_.empty() && writable_fds_.empty())
    {
      return {};
    }

    fd_set read_set;
    fd_set write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    int max_fd = -1;
    for (int fd : readable_fds_)
    {
      FD_SET(static_cast<SOCKET>(fd), &read_set);
      if (fd > max_fd)
      {
        max_fd = fd;
      }
    }
    for (int fd : writable_fds_)
    {
      FD_SET(static_cast<SOCKET>(fd), &write_set);
      if (fd > max_fd)
      {
        max_fd = fd;
      }
    }

    timeval timeout{};
    timeval *timeout_ptr = nullptr;
    if (timeout_ms >= 0)
    {
      timeout.tv_sec = timeout_ms / 1000;
      timeout.tv_usec = (timeout_ms % 1000) * 1000;
      timeout_ptr = &timeout;
    }

    int count = select(max_fd + 1, &read_set, &write_set, nullptr, timeout_ptr);
    if (count < 0)
    {
      int error = WSAGetLastError();
      if (error == WSAEINTR)
      {
        return {};
      }
      throw std::system_error(error, std::system_category(), system_message("select wait"));
    }

    std::vector<PollEvent> ready;
    ready.reserve(static_cast<size_t>(count));
    for (int fd : readable_fds_)
    {
      if (FD_ISSET(static_cast<SOCKET>(fd), &read_set))
      {
        ready.push_back({fd, true, false});
      }
    }
    for (int fd : writable_fds_)
    {
      if (FD_ISSET(static_cast<SOCKET>(fd), &write_set))
      {
        auto existing = std::find_if(ready.begin(), ready.end(), [fd](const PollEvent &event)
                                     { return event.fd == fd; });
        if (existing != ready.end())
        {
          existing->writable = true;
        }
        else
        {
          ready.push_back({fd, false, true});
        }
      }
    }
    return ready;
  }

#else
  // POSIX-Fallback: select

  void SelectPoller::add(int fd, bool readable, bool writable)
  {
    if (readable)
    {
      readable_fds_.insert(fd);
    }
    if (writable)
    {
      writable_fds_.insert(fd);
    }
  }

  void SelectPoller::update(int fd, bool readable, bool writable)
  {
    if (readable)
    {
      readable_fds_.insert(fd);
    }
    else
    {
      readable_fds_.erase(fd);
    }
    if (writable)
    {
      writable_fds_.insert(fd);
    }
    else
    {
      writable_fds_.erase(fd);
    }
  }

  void SelectPoller::remove(int fd)
  {
    readable_fds_.erase(fd);
    writable_fds_.erase(fd);
  }

  std::vector<PollEvent> SelectPoller::wait(int timeout_ms)
  {
    if (readable_fds_.empty() && writable_fds_.empty())
    {
      return {};
    }

    fd_set read_set;
    fd_set write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    int max_fd = -1;
    for (int fd : readable_fds_)
    {
      FD_SET(fd, &read_set);
      if (fd > max_fd)
      {
        max_fd = fd;
      }
    }
    for (int fd : writable_fds_)
    {
      FD_SET(fd, &write_set);
      if (fd > max_fd)
      {
        max_fd = fd;
      }
    }

    timeval timeout{};
    timeval *timeout_ptr = nullptr;
    if (timeout_ms >= 0)
    {
      timeout.tv_sec = timeout_ms / 1000;
      timeout.tv_usec = (timeout_ms % 1000) * 1000;
      timeout_ptr = &timeout;
    }

    int count = select(max_fd + 1, &read_set, &write_set, nullptr, timeout_ptr);
    if (count < 0)
    {
      if (errno == EINTR)
      {
        return {};
      }
      throw std::system_error(errno, std::system_category(), system_message("select wait"));
    }

    std::vector<PollEvent> ready;
    ready.reserve(static_cast<size_t>(count));
    for (int fd : readable_fds_)
    {
      if (FD_ISSET(fd, &read_set))
      {
        ready.push_back({fd, true, false});
      }
    }
    for (int fd : writable_fds_)
    {
      if (FD_ISSET(fd, &write_set))
      {
        auto existing = std::find_if(ready.begin(), ready.end(), [fd](const PollEvent &event)
                                     { return event.fd == fd; });
        if (existing != ready.end())
        {
          existing->writable = true;
        }
        else
        {
          ready.push_back({fd, false, true});
        }
      }
    }
    return ready;
  }

#endif

  std::unique_ptr<EventPoller> make_poller()
  {
#if defined(__linux__)
    return std::make_unique<EpollPoller>();
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    return std::make_unique<KqueuePoller>();
#else
    return std::make_unique<SelectPoller>();
#endif
  }

  // Platform-independent socket helpers
  int create_socket()
  {
#ifdef _WIN32
    static bool wsa_initialized = false;
    if (!wsa_initialized)
    {
      WSADATA wsa_data;
      if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
      {
        throw std::system_error(WSAGetLastError(), std::system_category(), "WSAStartup failed");
      }
      wsa_initialized = true;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
      throw std::system_error(WSAGetLastError(), std::system_category(), "socket failed");
    }
    return static_cast<int>(sock);
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
      throw std::system_error(errno, std::system_category(), "socket failed");
    }
    return sock;
#endif
  }

  void set_socket_reuse(int fd)
  {
    int reuse = 1;
#ifdef _WIN32
    if (setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&reuse), sizeof(reuse)) == SOCKET_ERROR)
    {
      throw std::system_error(WSAGetLastError(), std::system_category(), "setsockopt failed");
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
      throw std::system_error(errno, std::system_category(), "setsockopt failed");
    }
#endif
  }

  void set_non_blocking(int fd)
  {
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode) != 0)
    {
      throw std::system_error(WSAGetLastError(), std::system_category(), "ioctlsocket failed");
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
      throw std::system_error(errno, std::system_category(), "fcntl get failed");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
      throw std::system_error(errno, std::system_category(), "fcntl set failed");
    }
#endif
  }

  void close_socket(int fd)
  {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(fd));
#else
    close(fd);
#endif
  }

  int accept_connection(int listen_fd)
  {
    sockaddr_in client_addr{};
#ifdef _WIN32
    int client_len = sizeof(client_addr);
    SOCKET client_sock = accept(static_cast<SOCKET>(listen_fd),
                                reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    if (client_sock == INVALID_SOCKET)
    {
      int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK)
      {
        return -1; // No connection available
      }
      throw std::system_error(error, std::system_category(), "accept failed");
    }
    return static_cast<int>(client_sock);
#else
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    if (client_fd == -1)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return -1; // No connection available
      }
      throw std::system_error(errno, std::system_category(), "accept failed");
    }
    return client_fd;
#endif
  }

  int receive_data(int fd, char *buffer, int buffer_size)
  {
#ifdef _WIN32
    int bytes = recv(static_cast<SOCKET>(fd), buffer, buffer_size, 0);
    if (bytes == SOCKET_ERROR)
    {
      int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK)
      {
        return 0; // No data available
      }
      return -1; // Error
    }
    return bytes;
#else
    ssize_t bytes = recv(fd, buffer, buffer_size, 0);
    if (bytes < 0)
    {
      if (socket_pending_error(fd) == 0)
      {
        return 0; // No data available
      }
      return -1; // Error
    }
    return static_cast<int>(bytes);
#endif
  }

  int send_data(int fd, const char *data, int size)
  {
#ifdef _WIN32
    int written = send(static_cast<SOCKET>(fd), data, size, 0);
    if (written == SOCKET_ERROR)
    {
      int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK)
      {
        return 0; // Would block
      }
      return -1; // Error
    }
    return written;
#else
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
#endif
    ssize_t written = send(fd, data, size, flags);
    if (written < 0)
    {
      if (socket_pending_error(fd) == 0)
      {
        return 0; // Would block
      }
      return -1; // Error
    }
    return static_cast<int>(written);
#endif
  }

} // namespace event
