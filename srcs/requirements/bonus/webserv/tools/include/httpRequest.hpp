#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <optional>
#include "statusCodes.hpp"

#define MAX_HEADER_SIZE 64000
#define MAX_HEADER_LINE 8000
#define MAX_REQUEST_LINE 8050
#define MAX_URI 8000
#define MAX_CONT_LEN 1046364747436

enum ParseState {REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR};

struct parsedRequest
{
    std::string buffer;
    std::string method;
    std::string uri;
    std::string query;
    std::string http_v;
    std::map<std::string, std::string> headers;
    std::string body;
    size_t content_length;
    std::string content_type;
    int error_code;
    std::string error_info;
    bool chunked = false;
    bool MPFlag = false;
    std::string user_agent;
};

class HttpRequest 
{
    private:
    std::string buffer;
    std::string method;
    std::string uri;
    std::string query;
    std::string http_v;
    std::map<std::string, std::string> headers;
    std::string body;
    size_t content_length;
    std::string content_type;
    int error_code;
    std::string error_info;
    bool is_complete;
    ParseState state;
    bool chunked;
    bool MPFlag;

    bool validateEncodedURI(const std::string& str);
    void parseSL(std::string cont);
    void parseHeader(std::string cont);
    ParseState parseChunkedBody(std::string& buffer);
    ParseState parseMultipart();
    void parseMultipartHeaders(const std::string& head);
    void setError(ErrorCode type, std::string info);
    
    public:
    //ocf
    HttpRequest();
    HttpRequest(const HttpRequest& other);
    HttpRequest& operator=(const HttpRequest& other);
    ~HttpRequest();

    ParseState parseRequest(const char* data, size_t len);
    
    // Getters
    const std::string& getMethod() const;
    const std::string& getUri() const;
    const std::string getHeaderVal(const std::string& key) const;
    ParseState getState() const;
    const std::string getBuffer() const;
    parsedRequest getRequest();

    //print
    void printRequest();
    void printError();
    // helper
    void clear();
    void check_host();
    void check_cont_len();
    void check_transfer_enc();
};

std::string decodeURI(const std::string& str);
std::string decodeQuery(const std::string& str);
bool validateDecodedURI(const std::string& str);

bool validateHttpV(std::string str);
bool validateHeader(std::string key, std::string value);
void skipWhitespace(const std::string& str, size_t& i);

#endif
