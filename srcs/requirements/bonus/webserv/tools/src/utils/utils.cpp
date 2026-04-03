#include "../../include/utils.hpp"
#include "../../include/statusCodes.hpp"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <limits.h>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <map>

std::string addBackSlash(std::string path)
{
    if (path.back() != '/')
        path += '/';
    return path;
}

std::string generateFilename()
{
    static size_t count = 0;
    std::ostringstream oss;
    oss << std::time(nullptr) << "_" << count++;
    return oss.str();
}

std::string getExtUri(std::string uri)
{

    auto dot = uri.find_last_of(".");
    if (dot != std::string::npos)
        return uri.substr(dot);
    else
        return ".bin";
}

std::string getMimeExt(std::string mime)
{
    static std::map<std::string, std::string> mimeExt;

    if (mimeExt.empty())
    {
        mimeExt["text/html"]                        = ".html";
        mimeExt["text/css"]                         = ".css";
        mimeExt["application/javascript"]           = ".js";
        mimeExt["text/plain"]                       = ".txt";
        mimeExt["application/json"]                 = ".json";
        mimeExt["application/xml"]                  = ".xml";
        mimeExt["image/png"]                        = ".png";
        mimeExt["image/jpeg"]                       = ".jpg";
        mimeExt["image/jpg"]                        = ".jpg";
        mimeExt["image/gif"]                        = ".gif";
        mimeExt["image/x-icon"]                     = ".ico";
        mimeExt["image/svg+xml"]                    = ".svg";
        mimeExt["application/pdf"]                  = ".pdf";
        mimeExt["application/msword"]               = ".doc";
        mimeExt["application/vnd.ms-excel"]         = ".xls";
        mimeExt["application/vnd.ms-powerpoint"]    = ".ppt";
        mimeExt["audio/mpeg"]                       = ".mp3";
        mimeExt["audio/mp3"]                        = ".mp3";
        mimeExt["audio/wav"]                        = ".wav";
        mimeExt["audio/mp4"]                        = ".m4a";
        mimeExt["video/mp4"]                        = ".mp4";
        mimeExt["video/mpeg"]                       = ".mpeg";
        mimeExt["video/quicktime"]                  = ".mov";
        mimeExt["text/markdown"]                    = ".md";
    }
    auto it = mimeExt.find(mime);
    if (it != mimeExt.end())
        return it->second;
    else
        return ".bin";
}

std::string getMime(std::string path)
{
    static std::map<std::string, std::string> mimeTypes;

    if (mimeTypes.empty())
    {
        mimeTypes["html"] = "text/html";
        mimeTypes["htm"]  = "text/html";
        mimeTypes["css"]  = "text/css";
        mimeTypes["js"]   = "application/javascript";
        mimeTypes["txt"]  = "text/plain";
        mimeTypes["png"]  = "image/png";
        mimeTypes["jpg"]  = "image/jpeg";
        mimeTypes["jpeg"] = "image/jpeg";
        mimeTypes["gif"]  = "image/gif";
        mimeTypes["ico"]  = "image/x-icon";
        mimeTypes["svg"]  = "image/svg+xml";
        mimeTypes["json"] = "application/json";
        mimeTypes["pdf"]  = "application/pdf";
        mimeTypes["mpeg"] = "audio/mpeg";
        mimeTypes["wav"]  = "audio/wav";
        mimeTypes["m4a"]  = "audio/mp4";
    }

    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::string key = path.substr(dot + 1);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    auto it = mimeTypes.find(key);
    if (it != mimeTypes.end())
        return it->second;

    return "application/octet-stream";
}

std::string str_tolower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool isWithinFSRoot(const std::string& full_path, const std::string& allowed_root)
{
    try 
    {
        std::filesystem::path wc_root = std::filesystem::weakly_canonical(allowed_root);
        std::filesystem::path wc_full = std::filesystem::weakly_canonical(full_path);

        auto root_str = wc_root.string();
        auto full_str = wc_full.string();

        if (full_str == root_str)
            return true;

        if (!root_str.empty() && root_str.back() != '/')
            root_str += '/';

        return full_str.compare(0, root_str.size(), root_str) == 0;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return false;
    }
}

std::string htmlEscape(const std::string& str)
{
    std::string escaped;
    for (char c : str)
    {
        switch(c)
        {
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '&': escaped += "&amp;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c;
        }
    }
    return escaped;
}

std::string httpDate()
{
    std::time_t now = std::time(NULL);

    std::tm gmt;
    gmt = *std::gmtime(&now);

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &gmt);

    return std::string(buffer);
}

std::string generateErrorPage(int status, std::string info)
{
    std::ostringstream html;

    html << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>"
         << status << " " << info
         << "</title></head>\n"
         << "<body>\n"
         << "<h1>" << status << " " << info << "</h1>\n"
         << "<p>Error occurred while processing your request.</p>\n"
         << "</body>\n"
         << "</html>\n";

    return html.str();
}


std::string mapStatus(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";

        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";

        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 429: return "Too Many Requests";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";

        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";

        default:  return "Unknown Status";
    }
}

bool isDefaultEP(int status)
{
    return status == BadRequest || status == Forbidden || status == NotFound
            || status == MethodNotAllowed || status ==PayloadTooLarge || status == InternalServerError
            || status == NotImplemented || status == BadGateway || status == GatewayTimeout
            || status == HTTPVersionNotSupported;
}

bool isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

std::string resolveConfigPath(const std::string& path)
{
    if (path.empty())
        return path;

    std::filesystem::path p(path);
    if (p.is_relative())
        p = std::filesystem::current_path() / p;
    p = p.lexically_normal();

    return p.string();
}


bool isNumber(const std::string& str)
{
    if (str.empty())
        return false;
    return (str.find_first_not_of("0123456789") == std::string::npos);
}

bool isListen(const std::string& str)
{
    size_t colon = str.find(':');
    
    if (colon != std::string::npos)
    {
        std::string ip = str.substr(0, colon);
        std::string port = str.substr(colon + 1);
        
        return isValidIP(ip) && isPort(port);
    } 
    else
        return isPort(str);
}

bool isValidIP(const std::string& ip)
{
    if (ip.empty())
        return false;
    
    int dots = 0;
    for (char c : ip)
    {
        if (c == '.')
            dots++;
        else if (!std::isdigit(c))
            return false;
    }
    return dots == 3;
}

bool isPort(const std::string& str)
{
    if (!isNumber(str))
        return false;
    try 
    {
        int a = std::stoi(str);
        return a > 1 && a < 65535;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool isMethod(const std::string& str)
{
    if (str.empty())
        return false;

    return str == "GET" || str == "POST" || str == "DELETE";
}

// rn checking in config and server fails, we can switch to only host header check also
bool isDomainname(std::string str)
{
    if (str.empty() || str.length() > 253 || str.front() == '.' || str.back() == '.')
        return false;
    while (!str.empty())
    {
        auto dot = str.find('.');
        std::string label;
        if (dot != std::string::npos)
        {
            label = str.substr(0, dot);
            str.erase(0, dot + 1);
        }
        else
        {
            label = str;
            str.erase();
        }
        if (label.size() > 63 || label.size() == 0 || !std::isalnum(label[0]) || !std::isalnum(label.back()))
            return false;
        for (char c : label)
        {
            if (!std::isalnum(c) && c != '-')
            return false;
        }
    }
    return true;
}

bool isValidIPv6(const std::string& ip)
{
    if (ip.empty())
        return false;

    int ccount = 0;
    int dccount = 0;
    bool lastcol = false;

    for (size_t i = 0; i < ip.size(); ++i)
    {
        char c = ip[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            lastcol = false;
        else if (c == ':')
        {
            if (lastcol)
            {
                dccount++;
                if (dccount > 1)
                    return false;
            }
            ccount++;
            lastcol = true;
        }
        else
            return false;
    }
    return ccount > 0;
}

// eg: [2001:db8::ff00:42:8329]     [::1]    [2001:0db8:0000:0000:0000:ff00:0042:8329]
bool isIPv6Host(const std::string& host)
{
    if (host.empty() || host.front() != '[')
        return false;
    size_t close = host.find(']');
    if (close == std::string::npos)
        return false;
    std::string ip = host.substr(1, close - 1);
    if (!isValidIPv6(ip))
        return false;
    if (close + 1 == host.length())
        return true;
    else if (host[close + 1] == ':')
    {
        std::string portStr = host.substr(close + 2);
        return isPort(portStr);
    }
    return false;
}
