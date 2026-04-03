#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <vector>
#include <string>
#include "statusCodes.hpp"
#include "configParser.hpp"
#include "httpRequest.hpp"

struct ServerConfig;

enum Resource
{
    RED,
    CGI,
    FILES,
    DIRECTORY_LISTING,
    UPLOAD,
    ERR
};

struct Route
{
    Resource type;
    
    int status;
    std::string file_path;
    /* std::sring error_info; */
    std::string redirect_url;
    std::string cgi_path;
    std::string cgi_ext;
    std::string script_path;
    std::string path_info;
    std::string script_name;
    const LocationConfig* location;
    
    Route() : type(ERR), status(0), location(nullptr) {}
};


class Router
{
	private:
    const ServerConfig &server;
	const parsedRequest req;
    Route result;

	const LocationConfig* findLocation(const std::string& uri) const;
    bool isCGI(const LocationConfig* loc, const std::string& file_path);
	std::string mapURI(const LocationConfig *loc, const std::string &uri);
    std::string mapIndexPath(const std::string& dir, const LocationConfig* loc) const;
    bool resolveCgiPathInfo(const LocationConfig* loc, const std::string& safe_uri);
    void finalizeCgiRoute(const LocationConfig* loc,
                          const std::string& script_fs_path,
                          const std::string& script_uri,
                          const std::string& path_info);
	
	public:
    Router(const ServerConfig& server, const parsedRequest &req);
    ~Router();
	Route route();
};

#endif
