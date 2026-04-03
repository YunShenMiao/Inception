
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cerrno>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>

#include "../../include/server.hpp"
#include "../../include/router.hpp"
#include "../../include/requestHandler.hpp"
#include "../../include/httpResponse.hpp"
#include "../../include/utils.hpp"

namespace
{
int map_static_open_errno_to_status(int err)
{
    if (err == ENOENT || err == ENOTDIR)
        return NotFound;
    if (err == EACCES)
        return Forbidden;
    return InternalServerError;
}

std::string static_content_type(const std::string& file_path)
{
    std::string content_type = getMime(file_path);
    if (content_type.find("text/") == 0 || content_type == "application/javascript")
        content_type += "; charset=utf-8";
    return content_type;
}
}

/*************************************************************************/
/*                              SERVER                                   */
/*************************************************************************/

Server* Server::active_instance = nullptr;

Server::Server(ConfigParser &config):
    config(config),
    poller(event::make_poller()),
    cgi_manager(nullptr),
    signal_pipe{ -1, -1 },
    signal_pipe_initialized(false),
    bot_detection_config(BotDetection::getDefaultConfig()),
    captcha_bypass()
{
    cgi_manager = std::make_unique<CgiManager>(*poller);
    active_instance = this;
}

Server::~Server()
{
    if (cgi_manager)
        cgi_manager->shutdown();
    if (signal_pipe_initialized)
    {
        poller->remove(signal_pipe[0]);
        close(signal_pipe[0]);
        close(signal_pipe[1]);
        signal_pipe_initialized = false;
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGCHLD, &sa, nullptr);
    }
    if (active_instance == this)
        active_instance = nullptr;
}

void Server::init()
{
    const std::vector<ServerConfig>& servers = config.getServers();

    for (const auto& server : servers)
    {
        for (const auto& lp : server.listen_port)
        {
            int fd = -1;
            for (const auto& ls : listen_sockets)
            {
                if (ls.ip == lp.ip && ls.port == lp.port)
                {
                    fd = ls.fd;
                    break;
                }
            }
            if (fd < 0)
            {
                fd = create_and_listen(lp);
                if (fd < 0)
                    throw std::runtime_error("Failed to bind socket");
                listen_sockets.emplace_back(fd, lp.ip, lp.port);
                #ifdef DEBUG
                std::cout << "\nListening on http://"
                        << inet_ntoa(*(struct in_addr*)&lp.ip)
                        << ":" << lp.port << "\n";
                #endif
            }
            listen_fd_set.insert(fd);
        }
    }
    setup_signal_pipe();
}

void Server::run()
{

    while (true)
    {
        auto events = poller->wait(1000);
    
        for (const auto& event : events)
        {
            if (signal_pipe_initialized && event.fd == signal_pipe[0])
            {
                handle_signal_pipe();
                continue;
            }
            if (dispatch_cgi_event(event))
                continue;
            if (listen_fd_set.count(event.fd) > 0)
            {
                new_connection(event.fd);
                continue;
            }
            if (event.readable)
                read_client(event.fd);
            else if (event.writable)
                client_write(event.fd);
        } 
        close_drained_clients();
        check_timeouts(); 
    }
}

/*************************************************************************/
/*                       CONNECTION MANAGEMENT                           */
/*************************************************************************/

void Server::new_connection(int listen_fd)
{
    while (true)
    {
        int client_fd = event::accept_connection(listen_fd);
        if (client_fd == -1)
            break;  // No more connections
        
        // Find which IP/port this listen socket is on
        const ListenSocket* ls = get_listen_socket(listen_fd);
        if (!ls)
        {
            event::close_socket(client_fd);
            continue;
        }
        
        event::set_non_blocking(client_fd);
        poller->add(client_fd, true, false);
        
        auto inserted = clients.emplace(client_fd, ClientCon(client_fd, ls->ip, ls->port));
        if (inserted.second)
            capture_peername(client_fd, inserted.first->second);
        #ifdef DEBUG
        std::cout << "\nNew client " << client_fd << " connected to " << ls->port << "\n";
        #endif
    }
}

void Server::capture_peername(int client_fd, ClientCon& conn)
{
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(client_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
    {
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)))
            conn.remote_addr = buf;
        conn.remote_port_net = addr.sin_port;
    }
}

void Server::check_timeouts()
{
    time_t now = std::time(nullptr);
    std::vector<int> to_close;
    
    for (auto& [fd, conn] : clients)
    {
        int timeout = 60;
        if (conn.servConf && conn.servConf->timeout > 0)
            timeout = conn.servConf->timeout;

        if (now - conn.last_act > timeout)
        {
            conn.req = HttpRequest();
            to_close.push_back(fd);
        }
    }
    
    for (int fd : to_close)
        close_client(fd);

    if (cgi_manager)
        cgi_manager->checkTimeouts(now);
}

void Server::close_client(int client_fd)
{
    auto it = clients.find(client_fd);
    if (it == clients.end())
        return;

    if (it->second.upload_active)
    {
        it->second.upload_file.close();
        unlink(it->second.upload_path.c_str());
    }

    reset_static_file_stream(it->second);

    if (cgi_manager)
        cgi_manager->handleClientClose(client_fd);
    poller->remove(client_fd);
    clients.erase(it);
    event::close_socket(client_fd);
    #ifdef DEBUG
    std::cout << "\nClient " << client_fd << " disconnected\n";
    #endif
}

/*************************************************************************/
/*                          REQUEST PIPELINE                             */
/*************************************************************************/

bool Server::write_body(ClientCon& conn, const std::string& str, size_t len)
{
    if (conn.uploaded_bytes + len > MAX_CONT_LEN)
    {
        fallback_error(conn, PayloadTooLarge);
        conn.upload_file.close();
        unlink(conn.upload_path.c_str());
        return false;
    }

    conn.upload_file.write(str.data(), len);

    if (!conn.upload_file.good())
    {
        conn.upload_file.close();
        unlink(conn.upload_path.c_str());
        fallback_error(conn, InternalServerError);
        return false;
    }

    conn.upload_bytes_remaining -= len;
    conn.uploaded_bytes += len;
    return true;
}

bool Server::processBody(ClientCon &conn, const std::string &str, ParseState *state, size_t *to_write)
{
    if (conn.chunked)
    {
        std::string processed = processChunkedBody(conn, str, *state);
        if (*state == ERROR)
        {
            conn.upload_file.close();
            unlink(conn.upload_path.c_str());
            return false;
        }
        if (conn.MPFlag)
        {
            parseMultipart(conn, processed, processed.size());
            if (conn.multipart_state == MP_ERROR)
            {
                fallback_error(conn, InternalServerError);
                return false;
            }
        }
        else if (!write_body(conn, processed, processed.size()))
            return false;
    }
    else
    {
        *to_write = std::min(static_cast<size_t>(str.size()), conn.upload_bytes_remaining);
        if (conn.MPFlag)
        {
            parseMultipart(conn, str, *to_write);
            if (conn.multipart_state == MP_ERROR)
            {
                fallback_error(conn, InternalServerError);
                return false;
            }
        }
        else if (!write_body(conn, str, *to_write))
            return false;
    }
    return true;
}

void Server::read_client(int client_fd)
{
    auto it = clients.find(client_fd);
    if (it == clients.end())
        return;
    ClientCon& conn = it->second;
    conn.last_act = std::time(nullptr);

    std::array<char, 4096> buffer{};
    int bytes = event::receive_data(client_fd, buffer.data(), buffer.size());

    if (bytes <= 0)
    {

        close_client(client_fd);
        return;
    }

    // CGI body streaming
    if (conn.cgi_active && conn.cgi_streaming && cgi_manager)
    {
        std::string data(buffer.data(), bytes);
        if (conn.cgi_chunked)
        {
            ParseState state = BODY;
            std::string decoded = processChunkedBody(conn, data, state);
            if (state == ERROR)
            {
                cgi_manager->handleClientClose(client_fd);
                close_client(client_fd);
                return;
            }
            if (!decoded.empty())
                cgi_manager->feedStdin(client_fd, decoded);
            if (state == COMPLETE)
            {
                conn.cgi_streaming = false;
                cgi_manager->finishStdin(client_fd);
            }
        }
        else
        {
            size_t to_feed = std::min(static_cast<size_t>(bytes), conn.cgi_body_remaining);
            if (to_feed > 0)
            {
                cgi_manager->feedStdin(client_fd, std::string(buffer.data(), to_feed));
                conn.cgi_body_remaining -= to_feed;
            }
            if (conn.cgi_body_remaining == 0)
            {
                conn.cgi_streaming = false;
                cgi_manager->finishStdin(client_fd);
            }
        }
        return;
    }

    size_t to_write = 0;
    if (conn.upload_active)
    {
        ParseState state = BODY;
        std::string str(buffer.data(), bytes);
        if (!processBody(conn, str, &state, &to_write))
            return;

        if (conn.upload_bytes_remaining == 0 || state == COMPLETE || conn.multipart_state == MP_END)
            uploadComplete(conn);
    }
    if (static_cast<size_t>(bytes) > to_write)
    {
        ParseState state = conn.req.parseRequest(buffer.data() + to_write, bytes - to_write);
        if (state == ERROR)
        {
            int status = conn.req.getRequest().error_code;
            if (status <= 0) status = 400;
                fallback_error(conn, status);
            return;
        }

        if (state == REQUEST_LINE || state == HEADERS)
            return;

        if (state == BODY || state == COMPLETE)
        {
            #ifdef DEBUG
            conn.req.printRequest();
            #endif
            conn.http_v = conn.req.getRequest().http_v;
            std::string host = conn.req.getHeaderVal("host");
            conn.servConf = findServer(conn.ip, conn.port, host);
            if (!conn.servConf)
            {
                fallback_error(conn, 500);
                return;
            }

            if (state == BODY)
            {
                parsedRequest req = conn.req.getRequest();
                Router r(*conn.servConf, req);
                Route routy = r.route();
                bool upload_request = (routy.type == UPLOAD && req.method == "POST");
                bool cgi_request = (routy.type == CGI);
                if (!upload_request && !cgi_request)
                {
                    if (req.chunked && req.body.size() < req.content_length)
                        return;
                }
            }

            process_request(conn);

            if (conn.keep_alive && state == COMPLETE)
                conn.req.clear();

        }
    }
}

void Server::process_request(ClientCon& conn)
{
    parsedRequest req = conn.req.getRequest();

    std::string connection = str_tolower(conn.req.getHeaderVal("connection"));
    if (req.http_v == "HTTP/1.1")
        conn.keep_alive = connection != "close";
    else if (req.http_v == "HTTP/1.0")
        conn.keep_alive = connection == "keep-alive";

    Router r(*conn.servConf, req);
    Route routy = r.route();
    size_t max_body_size = conn.servConf->client_max_body_size;
    if (routy.location && routy.location->client_max_body_size > 0)
        max_body_size = routy.location->client_max_body_size;
    if (max_body_size > 0)
    {
        if ((!req.chunked && req.content_length > max_body_size) || req.body.size() > max_body_size)
        {
            fallback_error(conn, PayloadTooLarge);
            return;
        }
    }

    bool botdetect_enabled = !routy.location || routy.location->botdetect;

    if (botdetect_enabled)
    {
        std::string fingerprint = conn.remote_addr.empty() ? std::to_string(conn.ip) : conn.remote_addr;
        if (!req.user_agent.empty())
            fingerprint += "|" + req.user_agent;

        std::string bypass_cookie = captcha_bypass.extractTokenFromCookie(conn.req.getHeaderVal("cookie"));
        bool has_bypass = captcha_bypass.hasValidBypass(bypass_cookie, fingerprint);

        if (!has_bypass && handleBlockedBotRequest(conn, req, fingerprint))
            return;
    }

    if (routy.type == FILES && req.method == "GET")
    {
        start_static_file_stream(conn, routy, req);
        poller->update(conn.fd, false, true);
        conn.last_act = std::time(nullptr);
        return;
    }

    if (routy.type == CGI)
    {
        handleCgiRequest(conn, routy, req);
        return;
    }

    if (routy.type == UPLOAD && req.method == "POST")
    {
        startUpload(conn, routy, req);
        conn.last_act = std::time(nullptr);
        return;
    }

    RequestHandler handler(routy, req, *conn.servConf, conn.keep_alive);
    HttpResponse res = handler.handle();
    conn.response_out = res.buildResponse();
    conn.res_ready = true;

    poller->update(conn.fd, false, true);
    conn.last_act = std::time(nullptr);
}



bool Server::handleBlockedBotRequest(ClientCon& conn, const parsedRequest& req, const std::string& fingerprint)
{
    BotDetection::BotAnalysis bot_analysis = BotDetection::analyzeAndTrackRequest(
        fingerprint,
        req.uri,
        bot_request_history,
        bot_detection_config
    );

    if (bot_analysis.score != BotDetection::BotScore::BLOCKED)
        return false;

    if (captcha_bypass.isSolvedCaptchaPost(req.method, req.uri, req.body))
    {
        std::string token = captcha_bypass.createBypass(fingerprint);
        HttpResponse res(conn.keep_alive ? "keep-alive" : "close", req.http_v);
        res.setStatus(SeeOther);
        res.setHeader("Location", "/");
        res.setHeader("Set-Cookie",
                      "catsurf_clearance=" + token +
                      "; Path=/; Max-Age=3600; HttpOnly; SameSite=Lax");
        res.setHeader("Content-Length", "0");
        conn.response_out = res.buildResponse();
        conn.sent = 0;
        conn.res_ready = true;
        poller->update(conn.fd, false, true);
        conn.last_act = std::time(nullptr);
        return true;
    }

    std::string body = captcha_bypass.buildCaptchaPage();
    HttpResponse res(conn.keep_alive ? "keep-alive" : "close", req.http_v);
    res.setStatus(TooManyRequests);
    res.setHeader("Content-Type", "text/html; charset=utf-8");
    res.setHeader("Cache-Control", "no-store");
    res.setHeader("Content-Length", std::to_string(body.size()));
    res.setHeader("Retry-After", std::to_string(bot_detection_config.block_duration_seconds));
    res.setBody(body);

    conn.response_out = res.buildResponse();
    conn.sent = 0;
    conn.res_ready = true;
    poller->update(conn.fd, false, true);
    conn.last_act = std::time(nullptr);
    return true;
}

void Server::fallback_error(ClientCon& conn, int status)
{
    std::string body;
    if (!conn.servConf)
    {
        body = generateErrorPage(status, mapStatus(status));

        conn.response_out =
            "HTTP/1.1 " + std::to_string(status) + " " + mapStatus(status) + "\r\n"
            "Date: " + httpDate() + "\r\n"
            "Server: CatSurf\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + body;
        conn.keep_alive = false;
    }
    else
    {
        std::string ka = "close";
        if (conn.keep_alive)
            ka = "keep-alive";
        HttpResponse res(ka, conn.http_v);
        std::map<int, std::string>::const_iterator it = conn.servConf->error_page.find(status);

        if (it != conn.servConf->error_page.end())
        {
            std::string errorPagePath = conn.servConf->root;
            if (errorPagePath.back() != '/')
                errorPagePath += '/';
            errorPagePath += it->second;

            if (isWithinFSRoot(errorPagePath, conn.servConf->root))
            {
                if (!readFile(errorPagePath, body))
                    body = generateErrorPage(status, mapStatus(status));
            }
            else
                body = generateErrorPage(status, mapStatus(status));
        }
        else
            body = generateErrorPage(status, mapStatus(status));

        res.setHeader("Content-Type", "text/html");
        res.setHeader("Content-Length", std::to_string(body.size()));
        res.setStatus(status);
        res.setBody(body);

        conn.response_out = res.buildResponse();
    }
    conn.res_ready = true;
    poller->update(conn.fd, false, true);
}

const ServerConfig* Server::findServer(uint32_t ip, uint16_t port, const std::string& host_header)
{
	const std::vector<ServerConfig>& servers = config.getServers();
    const ServerConfig *firstMatch = nullptr;

    for (const auto& server : servers)
  	{
        for (const auto& lp : server.listen_port)
    	{
      		if (lp.ip == ip && lp.port == port)
      		{
        		if (!firstMatch)
                    firstMatch = &server; 
                if (!host_header.empty())
        		{
          			for (const auto& name : server.server_name)
          			{
                        if (name == str_tolower(host_header) || name == "_")
                            return &server;
          			}
        		}
      		}
    	}
  	}
    if (firstMatch)
    {
        return firstMatch;
    }
  	return nullptr;
}

/*************************************************************************/
/*                          RESPONSE PIPELINE                            */
/*************************************************************************/

//update(fd, read, write) -> event::update(fd, true, true);
void Server::client_write(int client_fd)
{
    auto it = clients.find(client_fd);
    if (it == clients.end())
        return;

    ClientCon& conn = it->second;

    if (!conn.res_ready && !conn.file_stream_active)
        return;

    if (!conn.response_out.empty())
    {
        if (conn.sent >= conn.response_out.size())
        {
            conn.response_out.clear();
            conn.sent = 0;
        }
        else
        {
            size_t remaining = conn.response_out.size() - conn.sent;
            int to_send = static_cast<int>(
                remaining > static_cast<size_t>(std::numeric_limits<int>::max())
                    ? std::numeric_limits<int>::max()
                    : remaining);
            int written = event::send_data(client_fd, conn.response_out.data() + conn.sent, to_send);

            if (written > 0)
            {
                conn.sent += static_cast<size_t>(written);
                conn.last_act = std::time(nullptr);
                drain_client_backpressure(conn);

                if (conn.sent == conn.response_out.size())
                {
                    conn.response_out.clear();
                    conn.sent = 0;
                    if (!conn.file_stream_active)
                        finalize_response_write(conn);
                }
                return;
            }
            if (written == 0)
            {
                return;
            }
            close_client(conn.fd);
            return;
        }
    }

    if (conn.file_stream_active)
    {
        write_static_file_chunk(conn);
        return;
    }

    if (conn.res_ready)
        finalize_response_write(conn);
}

bool Server::finalize_response_write(ClientCon& conn)
{
    conn.res_ready = false;

      if (conn.cgi_active)
    {
        // A drained socket buffer does not mean the CGI response is complete.
        // Keep reads disabled until the CGI manager finishes the process.
        poller->update(conn.fd, false, false);
        conn.last_act = std::time(nullptr);
        return true;
    }

    if (conn.close_after_send || !conn.keep_alive)
    {
        conn.close_after_send = false;
        close_client(conn.fd);
        return false;
    }
    conn.close_after_send = false;
    conn.req.clear();
    conn.last_act = std::time(nullptr);
    poller->update(conn.fd, true, false);
    return true;
}

void Server::close_drained_clients()
{
    std::vector<int> to_close;

    for (std::unordered_map<int, ClientCon>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        ClientCon& conn = it->second;
        if (conn.cgi_active)
            continue;
        if (!conn.close_after_send)
            continue;
        if (conn.res_ready)
            continue;
        if (!conn.response_out.empty())
            continue;
        if (conn.file_stream_active)
            continue;
        to_close.push_back(it->first);
    }

    for (std::vector<int>::iterator it = to_close.begin(); it != to_close.end(); ++it)
        close_client(*it);
}

/*************************************************************************/
/*                        STATIC FILESTREAMING                           */
/*************************************************************************/

bool Server::start_static_file_stream(ClientCon& conn, const Route& route, const parsedRequest& req)
{
    reset_static_file_stream(conn);
    conn.response_out.clear();
    conn.sent = 0;
    conn.res_ready = false;
    conn.close_after_send = false;

    auto queue_error = [&](int status)
    {
        HttpResponse res(conn.keep_alive ? "keep-alive" : "close", req.http_v);
        std::string body = generateErrorPage(status, mapStatus(status));
        res.setStatus(status);
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Content-Length", std::to_string(body.size()));
        res.setBody(body);
        conn.response_out = res.buildResponse();
        conn.sent = 0;
        conn.res_ready = true;
    };

    int open_flags = O_RDONLY;
#ifdef O_CLOEXEC
    open_flags |= O_CLOEXEC;
#endif
    int fd = open(route.file_path.c_str(), open_flags);
    if (fd < 0)
    {
        queue_error(map_static_open_errno_to_status(errno));
        return false;
    }

    struct stat st{};
    if (fstat(fd, &st) != 0)
    {
        int status = (errno == EACCES) ? Forbidden : InternalServerError;
        close(fd);
        queue_error(status);
        return false;
    }
    if (!S_ISREG(st.st_mode))
    {
        close(fd);
        queue_error(Forbidden);
        return false;
    }
    if (st.st_size < 0)
    {
        close(fd);
        queue_error(InternalServerError);
        return false;
    }

    unsigned long long file_size = static_cast<unsigned long long>(st.st_size);
    if (file_size > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
    {
        close(fd);
        queue_error(InternalServerError);
        return false;
    }

    HttpResponse res(conn.keep_alive ? "keep-alive" : "close", req.http_v);
    res.setStatus(Ok);
    res.setHeader("Content-Type", static_content_type(route.file_path));
    res.setHeader("Content-Length", std::to_string(file_size));

    conn.response_out = res.buildResponse();
    conn.sent = 0;
    conn.res_ready = true;
    conn.file_fd = fd;
    conn.file_stream_active = true;
    conn.file_bytes_remaining = static_cast<size_t>(file_size);
    conn.file_buf_len = 0;
    conn.file_buf_off = 0;
    return true;
}

bool Server::write_static_file_chunk(ClientCon& conn)
{
    if (!conn.file_stream_active)
        return finalize_response_write(conn);

    if (conn.file_buf_off >= conn.file_buf_len)
    {
        conn.file_buf_off = 0;
        conn.file_buf_len = 0;

        if (conn.file_bytes_remaining == 0)
        {
            reset_static_file_stream(conn);
            return finalize_response_write(conn);
        }

        size_t to_read = conn.file_buf.size();
        if (to_read > conn.file_bytes_remaining)
            to_read = conn.file_bytes_remaining;

        ssize_t read_bytes = read(conn.file_fd, conn.file_buf.data(), to_read);
        if (read_bytes < 0)
        {
            close_client(conn.fd);
            return false;
        }
        if (read_bytes == 0)
        {
            // Header is already out; avoid mismatched content-length by closing hard.
            close_client(conn.fd);
            return false;
        }

        conn.file_buf_len = static_cast<size_t>(read_bytes);
        conn.file_bytes_remaining -= conn.file_buf_len;
    }

    size_t pending = conn.file_buf_len - conn.file_buf_off;
    int written = event::send_data(conn.fd, conn.file_buf.data() + conn.file_buf_off, static_cast<int>(pending));
    if (written > 0)
    {
        conn.file_buf_off += static_cast<size_t>(written);
        conn.last_act = std::time(nullptr);
        drain_client_backpressure(conn);
        if (conn.file_buf_off == conn.file_buf_len && conn.file_bytes_remaining == 0)
        {
            reset_static_file_stream(conn);
            return finalize_response_write(conn);
        }
        return true;
    }
    if (written == 0)
        return true;
    close_client(conn.fd);
    return false;
}

void Server::reset_static_file_stream(ClientCon& conn)
{
    if (conn.file_fd >= 0)
        close(conn.file_fd);
    conn.file_fd = -1;
    conn.file_stream_active = false;
    conn.file_bytes_remaining = 0;
    conn.file_buf_len = 0;
    conn.file_buf_off = 0;
}

/*************************************************************************/
/*                                UPLOAD                                 */
/*************************************************************************/

void Server::resetUpload(ClientCon &conn)
{
    conn.upload_active = false;
    /* conn.upload_file; */
    conn.MPFlag = false;
    /* conn.multipart; */
    conn.multipart_state = MP_BOUNDARY;
    conn.MPCount = 0;
    conn.upload_path.clear();
    conn.chunk_buf.clear();
    conn.upload_bytes_remaining = 0;
    conn.uploaded_bytes = 0;
    conn.chunked = false;
}

void Server::uploadComplete(ClientCon& conn)
{
    conn.upload_file.close();
    HttpResponse res(conn.keep_alive ? "keep-alive" : "close", conn.http_v);
    res.setStatus(Created);
    res.setHeader("Location", conn.upload_path);
    conn.response_out = res.buildResponse();
    conn.res_ready = true;
    resetUpload(conn);
    poller->update(conn.fd, false, true);
}

void Server::parseMultipartHeaders(ClientCon &conn, const std::string &head)
{
    conn.multipart.mps.push_back(MPBody());

    auto start = head.find("name=\"");
    auto end = head.find("\"", start + 6);
    if (start != std::string::npos && end != std::string::npos)
        conn.multipart.mps[conn.multipart.part -1].name = head.substr(start + 6, end - (start + 6));

    start = head.find("filename=\"");
    end = head.find("\"", start + 10);
    if (start != std::string::npos && end != std::string::npos)
        conn.multipart.mps[conn.multipart.part -1].filename = head.substr(start + 10, end - (start + 10));

    start = head.find("Content-Type: ");
    end = head.find("\r\n", start + 14);
    if (start != std::string::npos && end != std::string::npos)
        conn.multipart.mps[conn.multipart.part -1].content_type = head.substr(start + 14, end - (start + 14));
}

void Server::parseMultipart(ClientCon& conn, std::string body, size_t to_write)
{
    conn.multipart.buffer.append(body.data(), to_write);
    
    if (conn.multipart_state == MP_BOUNDARY)
    {
        conn.multipart_state = MP_HEADER;
        conn.multipart.part++;
    }
    if (conn.multipart_state == MP_HEADER)
    {
        auto start = conn.multipart.buffer.find(conn.multipart.boundary + "\r\n");
        auto endH = conn.multipart.buffer.find("\r\n\r\n", start);
        if (start != std::string::npos && endH != std::string::npos)
        {
            std::string MPheaders = conn.multipart.buffer.substr(start, endH);
            parseMultipartHeaders(conn, MPheaders);
            conn.multipart.buffer.erase(0, endH + 4);
            conn.multipart_state = MP_BODY;

        if (!openUploadFile(conn, getMimeExt(conn.multipart.mps[conn.multipart.part -1].content_type)))
            return;
        }
        else
            return;
    }
    if (conn.multipart_state == MP_BODY)
    {
        std::string delimiter = conn.multipart.boundary;
        std::string final_delim  = "\r\n--" + delimiter + "--";

        size_t final_pos = conn.multipart.buffer.find(final_delim);
        if (final_pos != std::string::npos)
        {
            if (!write_body(conn, conn.multipart.buffer, final_pos))
                return;
            conn.upload_file.close();
            conn.multipart_state = MP_END;
            conn.multipart.buffer.clear();
            return;
        }

        size_t pos = conn.multipart.buffer.find(delimiter);

        if (pos != std::string::npos)
        {
            if (!write_body(conn, conn.multipart.buffer, pos))
                return;
    
            conn.multipart.buffer.erase(0, pos + 2);
            conn.multipart_state = MP_BOUNDARY;
        }

        size_t guard = delimiter.length() + 4;
        if (conn.multipart.buffer.size() > guard)
        {
            size_t safe = conn.multipart.buffer.size() - guard;
            if (!write_body(conn, conn.multipart.buffer, safe))
                return;
            conn.multipart.buffer.erase(0, safe);
        }

        return;
    }
}

bool Server::openUploadFile(ClientCon &conn, std::string ext)
{
    std::string fileName = generateFilename() + ext;
    conn.upload_path += fileName;
    conn.upload_file.open(conn.upload_path, std::ios::binary | std::ios::app);
    if (!conn.upload_file.is_open())
    {
        fallback_error(conn, InternalServerError);
        return false;
    }
    return true;
}

bool Server::setMPBoundary(ClientCon &conn, std::string content_type)
{
    conn.MPFlag = true;
    auto sep = content_type.find("=");

    if (sep != std::string::npos)
    {
        conn.multipart.boundary = content_type.substr(sep + 1);
        if (conn.multipart.boundary.front() == '\"' && conn.multipart.boundary.back() == '\"')
            conn.multipart.boundary = conn.multipart.boundary.substr(1, conn.multipart.boundary.size() - 2);
    }
    else
    {
        fallback_error(conn, BadRequest);
        return false;
    }
    return true;
}

void Server::startUpload(ClientCon& conn, const Route& route, const parsedRequest& req)
{
    if (!route.location || route.location->upload_path.empty())
    {
        fallback_error(conn, Forbidden);
        return;
    }
    if (req.MPFlag)
    {
        if (!setMPBoundary(conn, req.content_type))
            return;
    }
    conn.upload_path = addBackSlash(route.location->upload_path);
    if (!conn.MPFlag)
    { 
        if (!openUploadFile(conn, getExtUri(route.file_path)))
            return;
    }
    conn.upload_bytes_remaining = req.content_length;
    ParseState state = BODY;
    if (!req.body.empty())
    {
        if (req.chunked)
            conn.chunked = true;
        size_t to_write = 0;
        if (!processBody(conn, req.body, &state, &to_write))
            return;
        conn.req.clear();
    }      

    if (!conn.chunked && conn.upload_bytes_remaining == 0)
        uploadComplete(conn);
    else if (state == COMPLETE || conn.multipart_state == MP_END)
        uploadComplete(conn); 
    else
        conn.upload_active = true;
}

void Server::handleCgiRequest(ClientCon& conn, const Route& route, const parsedRequest& req)
{

    // For chunked requests, decode the initial body before launching CGI
    parsedRequest cgi_req = req;
    if (req.chunked && !req.body.empty())
    {
        conn.cgi_chunked = true;
        ParseState chunk_state = BODY;
        std::string decoded = processChunkedBody(conn, req.body, chunk_state);
        if (chunk_state == ERROR)
        {
            fallback_error(conn, BadRequest);
            return;
        }
        cgi_req.body = decoded;
    }

    if (!cgi_manager || !cgi_manager->launch(route, cgi_req, conn, *conn.servConf))
    {
        fallback_error(conn, BadGateway);
        return;
    }

    // Setup streaming for remaining body data
    ParseState state = conn.req.getState();
    if (state == BODY)
    {
        conn.cgi_streaming = true;
        conn.cgi_chunked = req.chunked;
        if (!req.chunked && req.content_length > req.body.size())
            conn.cgi_body_remaining = req.content_length - req.body.size();
        else if (!req.chunked)
        {
            conn.cgi_streaming = false;
            cgi_manager->finishStdin(conn.fd);
        }
    }
    else
    {
        // Body complete, close stdin immediately
        cgi_manager->finishStdin(conn.fd);
    }

    conn.last_act = std::time(nullptr);
}

std::string Server::processChunkedBody(ClientCon& conn, std::string buffer, ParseState& state)
{
    std::string decoded;
    conn.chunk_buf += buffer;
    state = BODY;

    while (true)
    {
        size_t crlf = conn.chunk_buf.find("\r\n");
        if (crlf == std::string::npos)
            break;

        std::string size_str = conn.chunk_buf.substr(0, crlf);
        size_t chunk_size = 0;
        try
        {
            chunk_size = std::stoul(size_str, nullptr, 16);
        }
        catch (std::exception &e) 
        { 
            fallback_error(conn, BadRequest); state = ERROR;
            return "";
        }

        if (chunk_size == 0)
        {
            if (conn.chunk_buf.size() < crlf + 4)
                break;
            conn.chunk_buf.erase(0, crlf + 4);
            state = COMPLETE;
            break;
        }

        if (conn.chunk_buf.size() < crlf + 2 + chunk_size + 2) 
            break;
        if (conn.chunk_buf[crlf + 2 + chunk_size] != '\r' || conn.chunk_buf[crlf + 2 + chunk_size + 1] != '\n')
        {
            fallback_error(conn, BadRequest);
            state = ERROR;
            return "";
        }

        decoded.append(conn.chunk_buf, crlf + 2, chunk_size);
        conn.uploaded_bytes += chunk_size;
        conn.chunk_buf.erase(0, crlf + 2 + chunk_size + 2);
    }

    return decoded;
}

/*************************************************************************/
/*                                SOCKET                                 */
/*************************************************************************/

const ListenSocket* Server::get_listen_socket(int fd) const
{
    for (const auto& ls : listen_sockets)
    {
        if (ls.fd == fd)
            return &ls;
    }
    return nullptr;
}

int Server::create_and_listen(const ListenPort& lp)
{
    int fd = event::create_socket();
    event::set_socket_reuse(fd);
    bind_and_listen(fd, lp.port, lp.ip);
    event::set_non_blocking(fd);
    poller->add(fd, true, false);
    return fd;
}

void Server::bind_and_listen(int fd, uint16_t port, uint32_t ip)
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

void Server::setup_signal_pipe()
{
    if (signal_pipe_initialized)
        return;

    if (pipe(signal_pipe) != 0)
        throw std::runtime_error("Failed to create signal pipe");

    for (int i = 0; i < 2; ++i)
    {
        int flags = fcntl(signal_pipe[i], F_GETFL, 0);
        if (flags == -1 || fcntl(signal_pipe[i], F_SETFL, flags | O_NONBLOCK) == -1)
        {
            close(signal_pipe[0]);
            close(signal_pipe[1]);
            throw std::runtime_error("Failed to configure signal pipe");
        }
    }

    poller->add(signal_pipe[0], true, false);
    signal_pipe_initialized = true;

    struct sigaction sa{};
    sa.sa_handler = Server::sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) != 0)
    {
        poller->remove(signal_pipe[0]);
        close(signal_pipe[0]);
        close(signal_pipe[1]);
        signal_pipe_initialized = false;
        throw std::runtime_error("Failed to install SIGCHLD handler");
    }
}

void Server::sigchld_handler(int)
{
    if (!active_instance || !active_instance->signal_pipe_initialized)
        return;
    char byte = 1;
    ssize_t r = write(active_instance->signal_pipe[1], &byte, 1);
    if (r > 0)
        return;
    if (r == 0)
        return;
    return;
}

void Server::handle_signal_pipe()
{
    if (!signal_pipe_initialized)
        return;

    char buffer[64];
    while (true)
    {
        ssize_t n = read(signal_pipe[0], buffer, sizeof(buffer));
        if (n <= 0)
            break;
    }
    reap_children();
}

/*************************************************************************/
/*                               CGI                                     */
/*************************************************************************/

void Server::reap_children()
{
    if (!cgi_manager)
        return;

    int status = 0;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        cgi_manager->handleChildExit(pid, status);
}

bool Server::dispatch_cgi_event(const event::PollEvent& ev)
{
    if (cgi_manager && cgi_manager->handlesFd(ev.fd))
    {
        cgi_manager->handleEvent(ev.fd, ev.readable, ev.writable);
        return true;
    }
    return false;
}

void Server::drain_client_backpressure(ClientCon& conn)
{
    if (cgi_manager && conn.cgi_active)
        cgi_manager->notifyClientWritable(conn.fd);
}
