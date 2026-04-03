#ifndef SERVER_HPP
#define SERVER_HPP

#include <array>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <ctime>
#include <memory>
#include <fstream>

#include "cgi.hpp"
#include "configParser.hpp"
#include "poller.h"
#include "httpRequest.hpp"
#include "botDetection.hpp"
#include "captchaBypass.hpp"

enum MPState {MP_BEGIN, MP_BOUNDARY, MP_HEADER, MP_BODY, MP_END, MP_ERROR};

struct MPBody
{
    std::string name;
    std::string filename;
    std::string content_type;
    std::string body;
};

struct MPFile
{
    std::string boundary;
    std::string buffer;
    int part = 0;
    std::vector<MPBody> mps;
};

struct ClientCon
{
    int fd;
    uint32_t ip;
    uint16_t port;
    std::string remote_addr;
    uint16_t remote_port_net;
    time_t last_act = 0;

    HttpRequest req;
    std::string http_v;
    const ServerConfig *servConf;

    std::string response_out;
    bool res_ready;
    bool keep_alive;
    size_t sent;

    int file_fd;
    bool file_stream_active;
    size_t file_bytes_remaining;
    std::array<char, 65536> file_buf;
    size_t file_buf_len;
    size_t file_buf_off;

    bool cgi_active;
    bool cgi_force_close;
    bool close_after_send;

    // CGI body streaming state
    bool cgi_streaming = false;
    size_t cgi_body_remaining = 0;
    bool cgi_chunked = false;

    bool upload_active = false;
    std::ofstream upload_file;
    bool MPFlag = false;
    MPFile multipart;
    MPState multipart_state = MP_BOUNDARY;
    int MPCount = 0;
    std::string upload_path;
    std::string chunk_buf;
    size_t upload_bytes_remaining = 0;
    size_t uploaded_bytes = 0;
    bool chunked = false;

    ClientCon(int fd, uint32_t ip, uint16_t port):
        fd(fd),
        ip(ip),
        port(port),
        remote_port_net(0),
        last_act(std::time(NULL)),
        req(),
        servConf(nullptr),
        res_ready(false),
        keep_alive(false),
        sent(0),
        file_fd(-1),
        file_stream_active(false),
        file_bytes_remaining(0),
        file_buf_len(0),
        file_buf_off(0),
        cgi_active(false),
        cgi_force_close(false),
        close_after_send(false)  {}
};

struct ListenSocket
{
    int fd;
    uint32_t ip;
    uint16_t port;

    ListenSocket(int fd, uint32_t ip, uint16_t port): fd(fd), ip(ip), port(port) {}
};

class Server
{
    private:
    const ConfigParser &config;
    std::unique_ptr<event::EventPoller> poller;
    std::unique_ptr<CgiManager> cgi_manager;
    int signal_pipe[2];
    bool signal_pipe_initialized;
    std::vector<ListenSocket> listen_sockets;
    std::unordered_set<int> listen_fd_set;
    std::unordered_map<int, ClientCon> clients;
    BotDetection::BotDetectionConfig bot_detection_config;
    std::unordered_map<std::string, std::vector<BotDetection::RequestSample>> bot_request_history;
    CaptchaBypass captcha_bypass;

    void new_connection(int listen_fd);
    void resetConnection(ClientCon& conn);
    void resetUpload(ClientCon &conn);
    bool processBody(ClientCon &conn, const std::string &str, ParseState *state, size_t *to_write);
    bool setMPBoundary(ClientCon &conn, std::string content_type);
    bool openUploadFile(ClientCon &conn, std::string ext);
    bool write_body(ClientCon &conn, const std::string &str, size_t len);
    void read_client(int client_fd);
    void parseMultipartHeaders(ClientCon &conn, const std::string &head);
    void parseMultipart(ClientCon& conn, std::string body, size_t to_write);
    std::string processChunkedBody(ClientCon& conn, std::string buffer, ParseState& state);
    void uploadComplete(ClientCon& conn);
    void startUpload(ClientCon& conn, const Route& route, const parsedRequest& req);
    void handleCgiRequest(ClientCon& conn, const Route& route, const parsedRequest& req);
    void client_write(int client_fd);
    const ServerConfig* findServer(uint32_t ip, uint16_t port, const std::string& host_header);
    void process_request(ClientCon& conn);
    void close_client(int client_fd);
    void check_timeouts();
    void fallback_error(ClientCon& conn, int status);
    bool start_static_file_stream(ClientCon& conn, const Route& route, const parsedRequest& req);
    bool write_static_file_chunk(ClientCon& conn);
    void reset_static_file_stream(ClientCon& conn);
    bool finalize_response_write(ClientCon& conn);
    void close_drained_clients();

    int create_and_listen(const ListenPort& lp);
    void bind_and_listen(int fd, uint16_t port, uint32_t ip);
    void setup_signal_pipe();
    void handle_signal_pipe();
    void reap_children();
    void drain_client_backpressure(ClientCon& conn);
    void capture_peername(int client_fd, ClientCon& conn);
    bool handleBlockedBotRequest(ClientCon& conn, const parsedRequest& req, const std::string& fingerprint);

    void send_response(int client_fd, bool keep_alive);
    void send_error_response(int client_fd, int status_code, std::string error_info);

    const ListenSocket* get_listen_socket(int fd) const;
    bool dispatch_cgi_event(const event::PollEvent& ev);
    static void sigchld_handler(int);
    static Server* active_instance;

    // int dom, int typ, int prot, int port, u_long interface, int bl
    public:
    Server(ConfigParser &config);
    Server(const Server& other) = delete;
    Server& operator=(const Server& other) = delete;
    Server(Server&&) = default;
    Server& operator=(Server&&) = delete;
    ~Server();

    void init();
    void run();
};

#endif
