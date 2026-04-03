#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "httpRequest.hpp"
#include "router.hpp"
#include "server.hpp"
#include "httpResponse.hpp"

class RequestHandler
{
    private:
    const Route &r;
    const parsedRequest &pr;
    const ServerConfig &sc;
    std::string ka;

    HttpResponse serveFile();
    HttpResponse deleteFile();
    HttpResponse uploadFile();
    HttpResponse handleFiles();
    HttpResponse handleCGI();
    HttpResponse handleDirectoryListing();
    HttpResponse handleError(int status);
    HttpResponse handleRedirect();

    public:
    RequestHandler(const Route &r, const parsedRequest &pr, const ServerConfig &sc, bool keep_alive);
    ~RequestHandler();
    HttpResponse handle();
};

std::string getMime(std::string path);
std::string getExtUri(std::string uri);
std::string getMimeExt(std::string mime);
bool readFile(const std::string& filepath, std::string& body);
#endif
