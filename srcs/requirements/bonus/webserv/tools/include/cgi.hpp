#ifndef CGI_HPP
#define CGI_HPP

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <ctime>

#include <sys/types.h>

#include "httpRequest.hpp"
#include "router.hpp"
#include "poller.h"

struct ClientCon;

enum class CgiTermination
{
    Normal,
    ClientGone,
    Timeout,
    ExecFailure,
    InvalidHeaders,
    ServerShutdown
};

class CgiManager
{
    public:
    explicit CgiManager(event::EventPoller& poller);
    ~CgiManager();

    bool launch(const Route& route,
                const parsedRequest& request,
                ClientCon& client,
                const ServerConfig& server);

    bool handlesFd(int fd) const;
    void handleEvent(int fd, bool readable, bool writable);
    void handleChildExit(pid_t pid, int status);
    void handleClientClose(int client_fd);
    void notifyClientWritable(int client_fd);
    void checkTimeouts(time_t now);
    void shutdown();

    // Streaming body support: feeding additional data to the CGI stdin
    void feedStdin(int client_fd, const std::string& data);
    void finishStdin(int client_fd);

    private:
    struct CgiProcess;

    CgiProcess* findByFd(int fd);
    void cleanupProcess(CgiProcess& proc, CgiTermination reason, int status_code = 500);
    void queueGatewayTimeout(CgiProcess& proc);
    void emitResponse(CgiProcess& proc);
    void forwardBody(CgiProcess& proc, const std::string& data);
    void finalizeChunk(CgiProcess& proc);
    void enforceBackpressure(CgiProcess& proc);

    event::EventPoller& poller;
    std::unordered_map<int, std::unique_ptr<CgiProcess>> processes;
    std::unordered_map<int, CgiProcess*> stdin_map;
    std::unordered_map<int, CgiProcess*> stderr_map;
    std::unordered_map<int, CgiProcess*> client_map;
    std::unordered_map<pid_t, CgiProcess*> pid_map;
};

#endif
