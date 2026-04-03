
#include "../../include/httpResponse.hpp"
#include "../../include/poller.h"
#include "../../include/utils.hpp"
#include <sstream>

HttpResponse::HttpResponse(std::string ka, std::string vers)
{
    http_v = vers;
    headers.insert({"Date", httpDate()});
    headers.insert({"Server", "CatSurf"});
    headers.insert({"Connection", ka});
    headers.insert({"Content-Length", "0"});
}

HttpResponse::~HttpResponse() {}

void HttpResponse::setHeader(std::string key, std::string value)
{
    auto it = headers.find(key);
    if (it != headers.end())
        headers[key] = value;
    else
        headers.insert({key, value});
}

void HttpResponse::removeHeader(const std::string& key)
{
    headers.erase(key);
}

void HttpResponse::setStatus(int stat)
{
    status_code = stat;
    status_info = mapStatus(stat);
}

void HttpResponse::setStatusText(const std::string& text)
{
    status_info = text;
}

void HttpResponse::setBody(std::string b)
{
    body = b;
}

void HttpResponse::setConnection(const std::string& value)
{
    setHeader("Connection", value);
}

std::string HttpResponse::buildResponse()
{
    std::ostringstream response;

    response << http_v << " "
             << status_code << " "
             << status_info << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        response << it->first << ": " << it->second << "\r\n";

    response << "\r\n";

    response << body;

    return response.str();
}
