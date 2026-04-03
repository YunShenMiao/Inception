#include "../../include/cgi.hpp"
#include "../../include/server.hpp"
#include "../../include/httpResponse.hpp"
#include "../../include/utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    const size_t kMaxHeaderBlock = 64000;
    const size_t kBackpressureThreshold = 128 * 1024;

    void set_nonblocking_fd(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            throw std::runtime_error("fcntl get failed");
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error("fcntl set failed");
    }

    std::string formatEnvVar(const std::string& key, const std::string& value)
    {
        return key + "=" + value;
    }

    std::string dirnameOf(const std::string& path)
    {
        std::filesystem::path p(path);
        if (p.has_parent_path())
            return p.parent_path().string();
        return ".";
    }

    std::string sanitizeHeaderKey(std::string key)
    {
        for (size_t i = 0; i < key.size(); ++i)
        {
            if (key[i] == '-')
                key[i] = '_';
            else
                key[i] = std::toupper(static_cast<unsigned char>(key[i]));
        }
        return key;
    }

    std::string canonicalizeHeaderKey(const std::string& key)
    {
        std::string result;
        result.reserve(key.size());
        bool capitalize = true;
        for (char ch : key)
        {
            if (ch == '-')
            {
                result.push_back('-');
                capitalize = true;
            }
            else
            {
                if (capitalize)
                    result.push_back(std::toupper(static_cast<unsigned char>(ch)));
                else
                    result.push_back(std::tolower(static_cast<unsigned char>(ch)));
                capitalize = false;
            }
        }
        return result;
    }
}

struct CgiManager::CgiProcess
{
    Route route;
    parsedRequest request;
    ClientCon* client;
    const ServerConfig* server;

    pid_t pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
    int stderr_fd = -1;
    bool stdin_closed = false;
    bool stdout_closed = false;
    bool stderr_closed = false;

    bool headers_complete = false;
    bool response_started = false;
    bool chunked_mode = false;
    bool has_content_length = false;
    bool waiting_chunk_terminator = false;
    bool terminated = false;
    bool child_reaped = false;
    bool pause_stdout = false;

    int status_code = 200;
    std::string status_text = "OK";

    size_t content_length_expected = 0;
    size_t content_length_sent = 0;

    std::string header_buffer;
    std::map<std::string, std::string> header_map;
    std::string stderr_buffer;

    std::string stdin_buffer;
    size_t stdin_offset = 0;

    time_t start_time = 0;
    time_t last_activity = 0;
    size_t absolute_timeout_ms = 30000;
    size_t idle_timeout_ms = 5000;

    std::string script_dir;
    std::string script_name;
    std::string path_info;

    bool force_close = false;

    CgiProcess(const Route& r,
               const parsedRequest& req,
               ClientCon& conn,
               const ServerConfig& serv):
        route(r),
        request(req),
        client(&conn),
        server(&serv),
        stdin_buffer(req.body),
        script_dir(dirnameOf(r.script_path)),
        script_name(r.script_name.empty() ? r.file_path : r.script_name),
        path_info(r.path_info)
    {
        absolute_timeout_ms = serv.cgi_timeout;
        idle_timeout_ms = serv.cgi_idle_timeout;
        start_time = std::time(nullptr);
        last_activity = start_time;
    }

    ~CgiProcess()
    {
        if (stdin_fd >= 0)
            close(stdin_fd);
        if (stdout_fd >= 0)
            close(stdout_fd);
        if (stderr_fd >= 0)
            close(stderr_fd);
    }

    bool stdinDone() const
    {
        return stdin_offset >= stdin_buffer.size();
    }

    bool isIdleExpired(time_t now) const
    {
        size_t elapsed_ms = static_cast<size_t>((now - last_activity) * 1000);
        return idle_timeout_ms > 0 && elapsed_ms > idle_timeout_ms;
    }

    bool isAbsoluteExpired(time_t now) const
    {
        size_t elapsed_ms = static_cast<size_t>((now - start_time) * 1000);
        return absolute_timeout_ms > 0 && elapsed_ms > absolute_timeout_ms;
    }
};

CgiManager::CgiManager(event::EventPoller& p): poller(p) {}

CgiManager::~CgiManager()
{
    shutdown();
}

bool CgiManager::launch(const Route& route,
                        const parsedRequest& request,
                        ClientCon& client,
                        const ServerConfig& server)
{
    if (route.script_path.empty())
        return false;

    auto proc = std::make_unique<CgiProcess>(route, request, client, server);

    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
    {
        perror("pipe");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return false;
    }

    if (pid == 0)
    {
        // child
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        std::filesystem::path cwd = proc->script_dir;
        if (!proc->script_dir.empty())
            chdir(proc->script_dir.c_str());

        std::vector<std::string> env_storage;
        env_storage.reserve(proc->request.headers.size() + 20);

        auto add_env = [&](const std::string& key, const std::string& value)
        {
            env_storage.push_back(formatEnvVar(key, value));
        };

        add_env("GATEWAY_INTERFACE", "CGI/1.1");
        add_env("REQUEST_METHOD", proc->request.method);
        add_env("QUERY_STRING", proc->request.query);
        add_env("CONTENT_LENGTH", proc->request.body.empty() ? "" : std::to_string(proc->request.body.size()));
        auto ct_it = proc->request.headers.find("content-type");
        if (ct_it != proc->request.headers.end())
            add_env("CONTENT_TYPE", ct_it->second);
        add_env("SERVER_PROTOCOL", proc->request.http_v);
        if (proc->server)
        {
            if (!proc->server->server_name.empty())
                add_env("SERVER_NAME", proc->server->server_name.front());
            if (!proc->server->listen_port.empty())
                add_env("SERVER_PORT", std::to_string(proc->server->listen_port.front().port));
        add_env("DOCUMENT_ROOT", proc->server->root);
        }
        add_env("SERVER_SOFTWARE", "CatSurf");
        add_env("REMOTE_ADDR", client.remote_addr.empty() ? "0.0.0.0" : client.remote_addr);
        add_env("REMOTE_PORT", std::to_string(ntohs(client.remote_port_net)));
        add_env("SCRIPT_NAME", proc->script_name);
        add_env("SCRIPT_FILENAME", proc->route.script_path);
        add_env("REQUEST_URI", proc->request.uri);
        add_env("PATH_INFO", proc->path_info);
        add_env("PATH_TRANSLATED", proc->route.script_path + proc->path_info);
        add_env("REDIRECT_STATUS", "200");

        for (const auto& header : proc->request.headers)
        {
            std::string key = header.first;
            std::string value = header.second;
            std::string upper = sanitizeHeaderKey(key);
            if (upper == "CONTENT_TYPE" || upper == "CONTENT_LENGTH")
                continue;
            if (upper == "TRANSFER_ENCODING")
                continue;
            add_env("HTTP_" + upper, value);
        }

        std::vector<char*> envp;
        envp.reserve(env_storage.size() + 1);
        for (auto& entry : env_storage)
            envp.push_back(entry.data());
        envp.push_back(nullptr);

        std::vector<std::string> args_storage;
        if (!route.cgi_path.empty())
        {
            args_storage.push_back(route.cgi_path);
            args_storage.push_back(route.script_path);
        }
        else
            args_storage.push_back(route.script_path);

        std::vector<char*> argv;
        argv.reserve(args_storage.size() + 1);
        for (auto& arg : args_storage)
            argv.push_back(arg.data());
        argv.push_back(nullptr);

        execve(argv[0], argv.data(), envp.data());
        perror("execve");
        _exit(126);
    }

    // parent
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    try
    {
        set_nonblocking_fd(stdin_pipe[1]);
        set_nonblocking_fd(stdout_pipe[0]);
        set_nonblocking_fd(stderr_pipe[0]);
    }
    catch (const std::exception& e)
    {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        kill(pid, SIGKILL);
        return false;
    }

    proc->pid = pid;
    proc->stdin_fd = stdin_pipe[1];
    proc->stdout_fd = stdout_pipe[0];
    proc->stderr_fd = stderr_pipe[0];
    proc->client->cgi_active = true;
    proc->client->cgi_force_close = false;

    poller.add(proc->stdout_fd, true, false);
    poller.add(proc->stderr_fd, true, false);

    bool expect_more_body = request.chunked || 
                            (request.content_length > request.body.size());
    
    if (!proc->stdinDone() || expect_more_body)
    {
        poller.add(proc->stdin_fd, false, true);
        stdin_map[proc->stdin_fd] = proc.get();
    }
    else
    {
        close(proc->stdin_fd);
        proc->stdin_fd = -1;
        proc->stdin_closed = true;
    }

    stderr_map[proc->stderr_fd] = proc.get();
    client_map[proc->client->fd] = proc.get();
    pid_map[proc->pid] = proc.get();
    processes[proc->stdout_fd] = std::move(proc);

    return true;
}

bool CgiManager::handlesFd(int fd) const
{
    if (processes.count(fd) > 0)
        return true;
    if (stdin_map.count(fd) > 0)
        return true;
    if (stderr_map.count(fd) > 0)
        return true;
    return false;
}

void CgiManager::handleEvent(int fd, bool readable, bool writable)
{
    auto it = processes.find(fd);
    if (it != processes.end())
    {
        if (readable)
        {
            CgiProcess& proc = *it->second;
            char buffer[4096];
            while (true)
            {
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0)
                {
                    proc.last_activity = std::time(nullptr);
                    if (!proc.headers_complete)
                    {
                        proc.header_buffer.append(buffer, n);
                        if (proc.header_buffer.size() > kMaxHeaderBlock)
                        {
                            cleanupProcess(proc, CgiTermination::InvalidHeaders, 502);
                            return;
                        }
                        size_t pos = proc.header_buffer.find("\r\n\r\n");
                        if (pos != std::string::npos)
                        {
                            std::string header_block = proc.header_buffer.substr(0, pos + 2);
                            std::string body = proc.header_buffer.substr(pos + 4);
                            proc.header_buffer.clear();
                            bool ok = true;
                            std::istringstream iss(header_block);
                            std::string line;
                            while (std::getline(iss, line))
                            {
                                if (!line.empty() && line.back() == '\r')
                                    line.pop_back();
                                if (line.empty())
                                    continue;
                                size_t colon = line.find(':');
                                if (colon == std::string::npos)
                                {
                                    ok = false;
                                    break;
                                }
                                std::string key = line.substr(0, colon);
                                std::string value = line.substr(colon + 1);
                                value.erase(0, value.find_first_not_of(" \t"));
                                proc.header_map[str_tolower(key)] = value;
                            }
                            if (!ok)
                            {
                                cleanupProcess(proc, CgiTermination::InvalidHeaders, 502);
                                return;
                            }
                            proc.headers_complete = true;
                            emitResponse(proc);
                            if (!body.empty())
                                forwardBody(proc, body);
                        }
                    }
                    else
                        forwardBody(proc, std::string(buffer, n));
                }
                else if (n == 0)
                {
                    proc.stdout_closed = true;
                    if (!proc.headers_complete)
                    {
                        cleanupProcess(proc, CgiTermination::InvalidHeaders, 502);
                        return;
                    }
                    if (proc.chunked_mode)
                        finalizeChunk(proc);
                    cleanupProcess(proc, CgiTermination::Normal);
                    return;
                }
                else
                    break;
            }
            enforceBackpressure(proc);
        }
        return;
    }

    auto in_it = stdin_map.find(fd);
    if (in_it != stdin_map.end() && writable)
    {
        CgiProcess& proc = *in_it->second;
        if (proc.stdin_fd < 0)
            return;
        if (proc.stdinDone())
        {
            poller.remove(proc.stdin_fd);
            close(proc.stdin_fd);
            proc.stdin_fd = -1;
            proc.stdin_closed = true;
            stdin_map.erase(in_it);
            return;
        }
        ssize_t n = write(proc.stdin_fd,
                          proc.stdin_buffer.data() + proc.stdin_offset,
                          proc.stdin_buffer.size() - proc.stdin_offset);
        if (n > 0)
        {
            proc.stdin_offset += static_cast<size_t>(n);
            proc.last_activity = std::time(nullptr);
            if (proc.stdinDone())
            {
                poller.remove(proc.stdin_fd);
                close(proc.stdin_fd);
                stdin_map.erase(in_it);
                proc.stdin_fd = -1;
                proc.stdin_closed = true;
            }
        }
        else if (n == 0)
        {
            // No progress for this writable notification ----> retry on next event.
            return;
        }
        else if (n < 0)
        {
            poller.remove(proc.stdin_fd);
            close(proc.stdin_fd);
            stdin_map.erase(in_it);
            proc.stdin_fd = -1;
            proc.stdin_closed = true;
        }
        return;
    }

    auto err_it = stderr_map.find(fd);
    if (err_it != stderr_map.end() && readable)
    {
        CgiProcess& proc = *err_it->second;
        char buffer[1024];
        while (true)
        {
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0)
            {
                proc.stderr_buffer.append(buffer, n);
                size_t pos = 0;
                while ((pos = proc.stderr_buffer.find('\n')) != std::string::npos)
                {
                    std::string line = proc.stderr_buffer.substr(0, pos);
                    proc.stderr_buffer.erase(0, pos + 1);
                    std::cerr << "[CGI " << proc.pid << "] " << line << "\n";
                }
            }
            else if (n == 0)
            {
                poller.remove(fd);
                close(fd);
                stderr_map.erase(err_it);
                proc.stderr_fd = -1;
                proc.stderr_closed = true;
                return;
            }
            else
                break;
        }
        return;
    }
}

void CgiManager::handleChildExit(pid_t pid, int status)
{
    auto it = pid_map.find(pid);
    if (it == pid_map.end())
        return;
    CgiProcess* proc = it->second;
    pid_map.erase(it);
    proc->child_reaped = true;
    if (WIFEXITED(status))
    {
        if (WEXITSTATUS(status) != 0 && !proc->headers_complete)
        {
            cleanupProcess(*proc, CgiTermination::ExecFailure, 502);
            return;
        }
    }
    if (proc->stdout_closed && proc->chunked_mode)
        finalizeChunk(*proc);
    if (proc->stdout_closed && !proc->chunked_mode)
    {
        proc->client->cgi_active = false;
        client_map.erase(proc->client->fd);
    }
}

void CgiManager::handleClientClose(int client_fd)
{
    auto it = client_map.find(client_fd);
    if (it == client_map.end())
        return;
    cleanupProcess(*it->second, CgiTermination::ClientGone, 499);
}

void CgiManager::notifyClientWritable(int client_fd)
{
    auto it = client_map.find(client_fd);
    if (it == client_map.end())
        return;
    CgiProcess& proc = *it->second;
    if (proc.pause_stdout)
    {
        proc.pause_stdout = false;
        try
        {
            poller.update(proc.stdout_fd, true, false);
        }
        catch (...) {}
    }
}

void CgiManager::checkTimeouts(time_t now)
{
    std::vector<CgiProcess*> timed_out;
    for (auto& entry : processes)
    {
        CgiProcess* proc = entry.second.get();
        if (proc->isIdleExpired(now) || proc->isAbsoluteExpired(now))
            timed_out.push_back(proc);
    }
    for (CgiProcess* proc : timed_out)
        queueGatewayTimeout(*proc);
}

void CgiManager::shutdown()
{
    std::vector<CgiProcess*> all;
    all.reserve(processes.size());
    for (auto& entry : processes)
        all.push_back(entry.second.get());
    for (CgiProcess* proc : all)
        cleanupProcess(*proc, CgiTermination::ServerShutdown, 500);
}

CgiManager::CgiProcess* CgiManager::findByFd(int fd)
{
    auto it = processes.find(fd);
    if (it != processes.end())
        return it->second.get();
    auto in = stdin_map.find(fd);
    if (in != stdin_map.end())
        return in->second;
    auto er = stderr_map.find(fd);
    if (er != stderr_map.end())
        return er->second;
    return nullptr;
}

void CgiManager::queueGatewayTimeout(CgiProcess& proc)
{
    cleanupProcess(proc, CgiTermination::Timeout, 504);
}

void CgiManager::cleanupProcess(CgiProcess& proc, CgiTermination reason, int status_code)
{
    if (proc.terminated)
        return;
    proc.terminated = true;
    proc.pause_stdout = false;

    int stdin_fd = proc.stdin_fd;
    int stdout_fd = proc.stdout_fd;
    int stderr_fd = proc.stderr_fd;
    pid_t pid = proc.pid;
    ClientCon* client = proc.client;

    // Keep the process object alive until cleanup is fully finished, even
    // after removing it from the lookup map.
    std::unique_ptr<CgiProcess> owned_proc;
    if (stdout_fd >= 0)
    {
        std::unordered_map<int, std::unique_ptr<CgiProcess>>::iterator proc_it = processes.find(stdout_fd);
        if (proc_it != processes.end())
        {
            owned_proc = std::move(proc_it->second);
            processes.erase(proc_it);
        }
    }
    if (!owned_proc)
    {
        for (std::unordered_map<int, std::unique_ptr<CgiProcess>>::iterator it = processes.begin();
             it != processes.end();
             ++it)
        {
            if (it->second.get() == &proc)
            {
                owned_proc = std::move(it->second);
                processes.erase(it);
                break;
            }
        }
    }

    if (stdin_fd >= 0)
        stdin_map.erase(stdin_fd);
    if (stderr_fd >= 0)
        stderr_map.erase(stderr_fd);
    if (client)
        client_map.erase(client->fd);
    if (pid > 0)
        pid_map.erase(pid);

    if (stdin_fd >= 0)
    {
        poller.remove(stdin_fd);
        close(stdin_fd);
        proc.stdin_fd = -1;
        proc.stdin_closed = true;
    }
    if (stdout_fd >= 0)
    {
        poller.remove(stdout_fd);
        close(stdout_fd);
        proc.stdout_fd = -1;
        proc.stdout_closed = true;
    }
    if (stderr_fd >= 0)
    {
        poller.remove(stderr_fd);
        close(stderr_fd);
        proc.stderr_fd = -1;
        proc.stderr_closed = true;
    }

    if (pid > 0)
    {
        kill(pid, SIGTERM);
        int child_status = 0;
        if (waitpid(pid, &child_status, WNOHANG) == 0)
            kill(pid, SIGKILL);
        proc.pid = -1;
    }

    bool client_gone = (reason == CgiTermination::ClientGone || reason == CgiTermination::ServerShutdown);
    if (client)
    {
        if (client_gone)
        {
            client->cgi_active = false;
            client->cgi_force_close = false;
        }
        else if (proc.response_started)
        {
            if (proc.force_close || reason != CgiTermination::Normal)
                client->keep_alive = false;
            client->cgi_force_close = false;
        }
        else
        {
            int final_status = (reason == CgiTermination::Normal) ? 502 : status_code;
            HttpResponse res("close", proc.request.http_v);
            res.setStatus(final_status);
            std::string body = generateErrorPage(final_status, mapStatus(final_status));
            res.setHeader("Content-Length", std::to_string(body.size()));
            res.setHeader("Content-Type", "text/html");
            res.setBody(body);
            client->response_out = res.buildResponse();
            client->res_ready = true;
            client->keep_alive = false;
            client->close_after_send = true;
            poller.update(client->fd, false, true);
        }
        client->cgi_active = false;
        client->cgi_force_close = false;

        if (client_gone)
            return;

        if (client->res_ready || !client->response_out.empty())
        {
            poller.update(client->fd, false, true);
        }
        else if (client->keep_alive)
        {
            poller.update(client->fd, true, false);
        }
        else
        {
            // closes the connection for EOF-terminated CGI responses.
            client->res_ready = true;
            poller.update(client->fd, false, true);
        }
    }
}

void CgiManager::emitResponse(CgiProcess& proc)
{
    int status = 200;
    std::string status_text = "OK";

    auto status_it = proc.header_map.find("status");
    if (status_it != proc.header_map.end())
    {
        std::istringstream ss(status_it->second);
        ss >> status;
        std::getline(ss, status_text);
        if (!status_text.empty() && status_text[0] == ' ')
            status_text.erase(0, 1);
        if (status_text.empty())
            status_text = mapStatus(status);
    }

    bool has_location = proc.header_map.find("location") != proc.header_map.end();
    if (has_location && status_it == proc.header_map.end())
    {
        status = 302;
        status_text = mapStatus(status);
    }

    auto cl_it = proc.header_map.find("content-length");
    if (cl_it != proc.header_map.end())
    {
        proc.has_content_length = true;
        proc.content_length_expected = static_cast<size_t>(std::strtoull(cl_it->second.c_str(), nullptr, 10));
    }

    proc.chunked_mode = false;
    if (!proc.has_content_length)
    {
        proc.force_close = true;
        proc.client->cgi_force_close = true;
        proc.client->close_after_send = true;
    }

    HttpResponse res(proc.client->keep_alive && !proc.force_close ? "keep-alive" : "close", proc.request.http_v);
    res.setStatus(status);
    if (!status_text.empty())
        res.setStatusText(status_text);
    res.removeHeader("Content-Length");

    for (const auto& header : proc.header_map)
    {
        std::string key = header.first;
        if (key == "status")
            continue;
        if (key == "content-length")
            res.setHeader("Content-Length", header.second);
        else if (key == "transfer-encoding")
            continue;
        else
            res.setHeader(canonicalizeHeaderKey(header.first), header.second);
    }

    if (!proc.has_content_length)
        res.setHeader("Connection", "close");

    if (proc.force_close)
        proc.client->keep_alive = false;

    proc.response_started = true;
    std::string head = res.buildResponse();
    proc.client->response_out += head;
    proc.client->res_ready = true;
    poller.update(proc.client->fd, false, true);
}

void CgiManager::forwardBody(CgiProcess& proc, const std::string& data)
{
    if (!proc.response_started || proc.terminated || proc.client == nullptr)
        return;
    proc.last_activity = std::time(nullptr);
    if (proc.chunked_mode)
    {
        std::ostringstream chunk;
        chunk << std::hex << data.size() << "\r\n";
        proc.client->response_out += chunk.str();
        proc.client->response_out.append(data);
        proc.client->response_out += "\r\n";
    }
    else
    {
        proc.client->response_out.append(data);
        proc.content_length_sent += data.size();
    }

    proc.client->res_ready = true;
    poller.update(proc.client->fd, false, true);
}

void CgiManager::finalizeChunk(CgiProcess& proc)
{
    if (!proc.response_started || proc.waiting_chunk_terminator)
        return;
    proc.waiting_chunk_terminator = true;
    proc.client->response_out += "0\r\n\r\n";
    proc.client->res_ready = true;
    poller.update(proc.client->fd, false, true);
}

void CgiManager::enforceBackpressure(CgiProcess& proc)
{
    size_t inflight = 0;
    if (proc.client)
    {
        inflight = proc.client->response_out.size();
        if (proc.client->sent < inflight)
            inflight -= proc.client->sent;
        else
            inflight = 0;
    }
    if (!proc.pause_stdout && inflight > kBackpressureThreshold)
    {
        proc.pause_stdout = true;
        poller.update(proc.stdout_fd, false, false);
    }
}

void CgiManager::feedStdin(int client_fd, const std::string& data)
{
    auto it = client_map.find(client_fd);
    if (it == client_map.end())
        return;
    CgiProcess& proc = *it->second;
    if (proc.stdin_closed || proc.stdin_fd < 0)
        return;

    proc.stdin_buffer.append(data);
    proc.last_activity = std::time(nullptr);

    if (stdin_map.find(proc.stdin_fd) != stdin_map.end())
    {
        try {
            poller.update(proc.stdin_fd, false, true);
        } catch (...) {}
    }
}

void CgiManager::finishStdin(int client_fd)
{
    auto it = client_map.find(client_fd);
    if (it == client_map.end())
        return;
    CgiProcess& proc = *it->second;
    if (proc.stdin_closed)
        return;

    if (proc.stdinDone() && proc.stdin_fd >= 0)
    {
        poller.remove(proc.stdin_fd);
        close(proc.stdin_fd);
        stdin_map.erase(proc.stdin_fd);
        proc.stdin_fd = -1;
        proc.stdin_closed = true;
    }
}
