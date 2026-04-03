#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

std::string addBackSlash(std::string path);
std::string generateFilename();
std::string getMime(std::string path);
std::string getMimeExt(std::string mime);
std::string getExtUri(std::string uri);
bool isNumber(const std::string& str);
bool isListen(const std::string& str);
bool isValidIP(const std::string& ip);
bool isPort(const std::string& str);
bool isMethod(const std::string& str);
bool isDomainname(std::string str);
bool isIPv6Host(const std::string& host);
bool isDirectory(const std::string& path);
std::string resolveConfigPath(const std::string& path);
bool isDefaultEP(int status);
std::string mapStatus(int code);
std::string httpDate();
std::string generateErrorPage(int status, std::string info);
std::string htmlEscape(const std::string& str);
bool isWithinFSRoot(const std::string& full_path, const std::string& allowed_root);
std::string str_tolower(std::string s);

#endif