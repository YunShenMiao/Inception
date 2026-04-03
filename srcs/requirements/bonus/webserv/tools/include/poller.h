
#ifndef CATSURF_POLLER_H
#define CATSURF_POLLER_H

#include <cerrno>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>
#include "configParser.hpp"

#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/event.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/select.h>
#endif

namespace event
{

  struct PollEvent
  {
    int fd;
    bool readable;
    bool writable;
  };

  class EventPoller
  {
  public:
    virtual ~EventPoller() = default;
    virtual void add(int fd, bool readable, bool writable) = 0;
    virtual void update(int fd, bool readable, bool writable) = 0;
    virtual void remove(int fd) = 0;
    virtual std::vector<PollEvent> wait(int timeout_ms) = 0;
  };

#if defined(__linux__)
  class EpollPoller final : public EventPoller
  {
  public:
    EpollPoller();
    ~EpollPoller() override;

    void add(int fd, bool readable, bool writable) override;
    void update(int fd, bool readable, bool writable) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeout_ms) override;

  private:
    int epoll_fd_;
  };
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  class KqueuePoller final : public EventPoller
  {
  public:
    KqueuePoller();
    ~KqueuePoller() override;

    void add(int fd, bool readable, bool writable) override;
    void update(int fd, bool readable, bool writable) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeout_ms) override;

  private:
    int kqueue_fd_;
  };
#else
  class SelectPoller final : public EventPoller
  {
  public:
    SelectPoller() = default;
    void add(int fd, bool readable, bool writable) override;
    void update(int fd, bool readable, bool writable) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeout_ms) override;

  private:
    std::unordered_set<int> readable_fds_;
    std::unordered_set<int> writable_fds_;
  };
#endif

  // liefert poller-Implementierung für das aktuelle OS.
  std::unique_ptr<EventPoller> make_poller();

  // socket helpers
  // wrapper kapseln plattformspezifische unterschiede (z.b WSAStartup, closesocket).
  int create_socket();
  void set_socket_reuse(int fd);
  void set_non_blocking(int fd);
  void close_socket(int fd);
  int accept_connection(int listen_fd);
  int receive_data(int fd, char *buffer, int buffer_size);
  int send_data(int fd, const char *data, int size);

} // namespace event

#endif // CATSURF_POLLER_H
