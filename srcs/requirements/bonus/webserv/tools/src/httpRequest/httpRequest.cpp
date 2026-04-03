#include "../../include/httpRequest.hpp"
#include "../../include/configParser.hpp"
#include "../../include/utils.hpp"
#include <iostream>
#include <cctype>
#include <algorithm>

HttpRequest::HttpRequest (): content_length(0), error_code(0), is_complete(false), state(REQUEST_LINE), chunked(false), MPFlag(false){}

HttpRequest::HttpRequest(const HttpRequest& other): buffer(other.buffer), method(other.method), uri(other.uri), query(other.query), http_v(other.http_v), headers(other.headers), body(other.body), content_length(other.content_length), error_code(other.error_code), error_info(other.error_info), is_complete(other.is_complete), state(other.state), chunked(other.chunked), MPFlag(other.MPFlag) {}

HttpRequest& HttpRequest::operator=(const HttpRequest& other)
{
    if (this != &other)
    {
        buffer = other.buffer;
        method = other.method;
        uri = other.uri;
        query = other.query;
        http_v = other.http_v;
        headers = other.headers;
        body = other.body;
        content_length = other.content_length;
        error_code = other.error_code;
        is_complete = other.is_complete;
        chunked = other.chunked;
        state = other.state;
        error_info = other.error_info;
        MPFlag = other.MPFlag;
        chunked = other.chunked;
    }
    return *this;
}

HttpRequest::~HttpRequest() {}

/***********************************************************/
/*                          GETTER                         */
/***********************************************************/

const std::string& HttpRequest::getMethod() const 
{
    return method;
}

ParseState HttpRequest::getState() const
{
    return state;
}

const std::string HttpRequest::getBuffer() const
{
    return buffer;
}

const std::string& HttpRequest::getUri() const
{
    return uri;
}

const std::string HttpRequest::getHeaderVal(const std::string& key) const
{
    auto it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}

parsedRequest HttpRequest::getRequest()
{
    parsedRequest req;
    req.buffer = buffer;
    req.method = method;
    req.uri = uri;
    req.query = query;
    req.http_v = http_v;
    req.headers = headers;
    req.body = body;
    req.content_length = content_length;
    req.error_code = error_code;
    req.error_info = error_info;
    req.content_type = content_type;
    req.chunked = chunked;
    req.MPFlag = MPFlag;

    auto ua_it = headers.find("user-agent");
    if (ua_it != headers.end()) {
        req.user_agent = ua_it->second;
    }
    
    return req;
}

/***********************************************************/
/*                          HELPER                         */
/***********************************************************/

void HttpRequest::printRequest()
{
    std::cout << "\n=== Parsed Request ===\n";
    std::cout << "Method: " << method << "\n";
    std::cout << "URI: " << uri << "\n";
    std::cout << "Version: " << http_v << "\n";

    std::cout << "\nHeaders:\n";
    for (auto it = headers.begin(); it != headers.end(); ++it)
        std::cout << it->first << " => " << it->second << std::endl;

    std::cout << "======================\n\n";
}

void HttpRequest::printError()
{
    std::cout << "ErrorCode: " << error_code << std::endl;
    std::cout << "ErrorInfo: " << error_info << std::endl;
}

void HttpRequest::setError(ErrorCode type, std::string info)
{
    error_code = type;
    error_info = info;
    state = ERROR;
    throw std::runtime_error(info);
}

void skipWhitespace(const std::string& str, size_t& i)
{
    while (i < str.size() && (str[i] == ' ' || str[i] == '\t'))
        i++;
}

void HttpRequest::clear()
{
    method.clear();
    uri.clear();
    query.clear();
    http_v.clear();
    headers.clear();
    body.clear();
    content_length = 0;
    error_code = 0;
    error_info.clear();
    is_complete = false;
    chunked = false;
    state = REQUEST_LINE;
}

/***********************************************************/
/*                      VALIDATION                         */
/***********************************************************/

bool HttpRequest::validateEncodedURI(const std::string& str)
{
    if (str.empty() || str.size() > MAX_URI)
        return false;
    if (str[0] != '/')
        return false;

    std::string allowed = "-._~/&=:@!$()*+,;%?";
    for (size_t i = 0; i < str.size(); i++)
    {
        if (!isalnum(static_cast<unsigned char>(str[i])) && allowed.find(str[i]) == std::string::npos)
            return false;
        if (str[i] == '%')
        {
            if (i + 2 >= str.size() || 
                !isxdigit(str[i+1]) || !isxdigit(str[i+2]))
                return false;
            i += 2;
        }
    } 
    return true;
}

std::string decodeQuery(const std::string& str)
{
    std::string res;
    res.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%')
        {
            char hex[3] = { str[i + 1], str[i + 2], '\0' };
            unsigned char c = static_cast<unsigned char>(std::strtol(hex, NULL, 16));
            if (c == '\0')
                throw std::runtime_error("null byte");

            res += c;
            i += 2;
        }
        else if (str[i] == '+')
            res += ' ';
        else
            res += str[i];
    }
    return res;
}


std::string decodeURI(const std::string& str)
{
    std::string res;
    res.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%')
        {
            char hex[3] = {str[i + 1], str[i + 2], '\0'};
            unsigned char c = static_cast<unsigned char>(std::strtol(hex, NULL, 16));
            if (c < 0x20 || c == 0x7F || c == '\\')
                throw std::runtime_error("invalid decoded char");

            res += c;
            i += 2;
        }
        else
            res += str[i];
    }
    return res;
}

bool validateHttpV(std::string str)
{
    if (str.empty())
        return false;
    else if (str != "HTTP/1.1" && str != "HTTP/1.0")
        return false;
    return true;
}

bool validateHeader(std::string key, std::string value)
{
    std::string allowed = "!#$%&'*+-.^_|~`";
    for (size_t i = 0; i < key.size(); i++)
    {
        if (!isalnum(static_cast<unsigned char>(key[i])) && allowed.find(key[i]) == std::string::npos)
            return false;
    }
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!isprint(static_cast<unsigned char>(value[i])) && value[i] != '\t')
            return false;
    }
    return true;
}

void HttpRequest::check_host()
{
    if (http_v == "HTTP/1.1" && getHeaderVal("host").empty())
        setError(BadRequest, "Missing Host header");
    if (!getHeaderVal("host").empty())
    {
        std::string val = getHeaderVal("host");
        auto ddot = val.find(':');
        if (ddot != std::string::npos)
        {
            val.erase(ddot);
            headers["host"] = val;
        }
            
        if (!isListen(val) && !isDomainname(val) && !isIPv6Host(val))
            setError(BadRequest, "Invalid host value");
    }
}

/***********************************************************/
/*                        PARSING                          */
/***********************************************************/

void HttpRequest::parseSL(std::string cont)
{
    while (cont.size() >= 2 && cont[0] == '\r' && cont[1] == '\n')
        cont.erase(0, 2);

    size_t start = 0;
    size_t end = cont.find(' ', start);

    if (end == std::string::npos)
        setError(BadRequest, "Invalid Request line");
    method = cont.substr(start, end);
    if (!isMethod(method))
        setError(NotImplemented, "Unsupported HTTP method");

    start = end + 1;
    end = cont.find(' ', start);
    if (end == std::string::npos)
        setError(BadRequest, "Invalid Request line");
    std::string raw_uri = cont.substr(start, end - start);
    if (raw_uri.size() > MAX_URI)
        setError(URITooLong, "URI too long");
    if (cont.size() > MAX_REQUEST_LINE)
        setError(BadRequest, "invalid or too long request line");
    std::string raw_query;
    size_t qpos = raw_uri.find('?');
    if (qpos != std::string::npos)
    {
        raw_query = raw_uri.substr(qpos + 1);
        raw_uri = raw_uri.substr(0, qpos);
    }

    if (!validateEncodedURI(raw_uri))
        setError(BadRequest, "Invalid URI in Request line");
    try
    {
        uri = decodeURI(raw_uri);
        if (!raw_query.empty())
            query = decodeQuery(raw_query);
        else
            query.clear();
    }
    catch (std::exception &e)
    {
        setError(BadRequest, "Invalid URI in Request line");
    }
    start = end + 1;
    http_v = cont.substr(start);
    if (!validateHttpV(http_v))
        setError(HTTPVersionNotSupported, "Invalid HTTP version in Request line");
}

void HttpRequest::check_cont_len()
{
    if (!getHeaderVal("content-length").empty())
    {
        try
        {
            if (!isNumber(getHeaderVal("content-length")))
                setError(BadRequest, "Invalid Content-Length");
            long long cl = std::stoll(getHeaderVal("content-length"));
            if (cl < 0)
                setError(BadRequest, "Invalid Content-Length");
            else if (cl > MAX_CONT_LEN)
                setError(PayloadTooLarge, "Payload too large");
            content_length = static_cast<size_t>(cl);
        }
        catch (std::exception &e)
        {
            setError(BadRequest, "Invalid Content-Length");
        }
    }
}

void HttpRequest::parseHeader(std::string cont)
{
    size_t i = 0;
    while (i < cont.size())
    {
        size_t key_end = cont.find(':', i);
        if (key_end == std::string::npos || key_end == i)
            setError(BadRequest, "Invalid header");
        std::string key = str_tolower(cont.substr(i, key_end - i));
        i = key_end + 1;
        skipWhitespace(cont, i);
        if ((key == "host" && !getHeaderVal("host").empty()) || (key == "content-length" && !getHeaderVal("content-length").empty()))
            setError(BadRequest, "Multiple Host || Content-length header");
        
        size_t value_end = cont.find("\r\n", i);
        std::string value;
        if (value_end == std::string::npos)
            value = cont.substr(i);
        else
            value = cont.substr(i, value_end - i);
        if ((key.size() + value.size()) > MAX_HEADER_LINE)
            setError(BadRequest, "Header line too long");

        headers.insert({key, value});

        if (!validateHeader(key, value))
            setError(BadRequest, "Invalid header");
        if (value_end == std::string::npos || i == cont.size())
            break;

        i = value_end + 2;
    }
    if (!getHeaderVal("content-length").empty() && !getHeaderVal("transfer-encoding").empty())
        setError(BadRequest, "both header fields 'content-length' & 'transfer-encoding' present");
    if (!getHeaderVal("content-type").empty())
        content_type = getHeaderVal("content-type");
    check_transfer_enc();
    check_host();
    check_cont_len();
}

void HttpRequest::check_transfer_enc()
{
    if (!getHeaderVal("transfer-encoding").empty())
    {
        if (http_v == "HTTP/1.0")
            setError(NotImplemented, "Transfer-Encoding not supported in HTTP/1.0");
        if ((str_tolower(getHeaderVal("transfer-encoding")).find("chunked") == std::string::npos))
            setError(BadRequest, "Unsupported Transfer-Encoding");
        else chunked = true;
    }
}

ParseState HttpRequest::parseRequest(const char* data, size_t len)
{
    buffer.append(data, len);
    while (state != COMPLETE && state != ERROR)
    {
        if (state == REQUEST_LINE) 
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                return state;
            try
            {
                parseSL(buffer.substr(0, pos));
                buffer.erase(0, pos + 2);
                state = HEADERS;
            }
            catch (std::exception& e)
            {
                return state;
            }
        }
        else if (state == HEADERS) 
        {
            if (buffer.size() > MAX_HEADER_SIZE)
            {
                setError(BadRequest, "invalid header");
                return state;
            }
            size_t pos = std::string::npos;
            size_t consumed = 0;

            if (buffer.size() >= 2 && buffer.compare(0, 2, "\r\n") == 0)
            {
                pos = 0;
                consumed = 2;
            }
            else
            {
                pos = buffer.find("\r\n\r\n");
                consumed = 4;
            }
            if (pos == std::string::npos)
                return state;
            try
            {
                parseHeader(buffer.substr(0, pos));
                buffer.erase(0, pos + consumed);
                if (content_length == 0 && !chunked)
                    state = COMPLETE;
                else
                        state = BODY;
            }
            catch (std::exception &e)
            {
                return state;
            } 
        }
        else if (state == BODY)
        {
            if (!content_type.empty() && content_type.find("multipart/form-data") != std::string::npos)
                MPFlag = true;
            if (chunked)
                body = buffer;
            else
            {
                body = buffer.substr(0, content_length);
                buffer.erase(0, content_length);
            }
            return state;
            }
    }
    return state;
}
