#include "../../include/configParser.hpp"
#include <iostream>
#include <iomanip>

#define RED   "\033[31m"
#define BLUE  "\033[34m"
#define RESET "\033[0m"

void ConfigParser::printGlobalConfig() const
{
    std::cout << BLUE << "Global Config:" << RESET << std::endl;
    std::cout << std::setw(18) << "error_log:" << global_config.error_log << std::endl;
    std::cout << std::setw(18) << "pid:" << global_config.pid << std::endl;
    std::cout << RED << "------------------------------------------------" << RESET << std::endl;
}

void ConfigParser::printList(const std::vector<std::string>& list) const
{
    for (size_t i = 0; i < list.size(); ++i)
    {
        std::cout << list[i];
        if (i + 1 < list.size())
            std::cout << ", ";
    }
}

void ConfigParser::printMap(const std::map<int, std::string>& m) const
{
    size_t count = 0;
    for (const auto& pair : m)
    {
        std::cout << pair.first << " -> " << pair.second;
        if (++count < m.size())
            std::cout << ", ";
    }
}

void ConfigParser::printLocation(const LocationConfig& loc, size_t idx) const
{
    std::cout << BLUE << "LocationConfig " << idx << ":" << RESET << std::endl;
    std::cout << std::setw(18) << "path:" << loc.path << std::endl;
    std::cout << std::setw(18) << "root:" << loc.root << std::endl;
    std::cout << std::setw(18) << "autoindex:" << loc.autoindex << std::endl;
    std::cout << std::setw(18) << "botdetect:" << loc.botdetect << std::endl;
    std::cout << std::setw(18) << "max_size:" << loc.client_max_body_size << std::endl;
    std::cout << std::setw(18) << "cgi_path:" << loc.cgi_path << std::endl;
    std::cout << std::setw(18) << "upload_path:" << loc.upload_path << std::endl;

    std::cout << std::setw(18) << "index files:";
    printList(loc.index);
    std::cout << std::endl;

    std::cout << std::setw(18) << "cgi extensions:";
    printList(loc.cgi_extension);
    std::cout << std::endl;

    std::cout << std::setw(18) << "allowed methods:";
    printList(loc.allow_methods);
    std::cout << std::endl;

    std::cout << std::setw(18) << "return:";
    printList(loc.return_);
    std::cout << std::endl;

    std::cout << RED << "------------------------------------------------" << RESET << std::endl;
}

void ConfigParser::printServerConfig(const ServerConfig& server, size_t idx) const
{
    std::cout << BLUE << "Server " << idx << ":" << RESET << std::endl;
    std::cout << std::setw(18) << "root:" << server.root << std::endl;
    std::cout << std::setw(18) << "max_size:" << server.client_max_body_size << std::endl;
    std::cout << std::setw(18) << "timeout:" << server.timeout << std::endl;

    std::cout << std::setw(18) << "index files:";
    printList(server.index);
    std::cout << std::endl;
    std::cout << std::setw(18) << "listen:";
/*     printList(server.listen_port);
    std::cout << std::endl; */

    std::cout << std::setw(18) << "server_name:";
    printList(server.server_name);
    std::cout << std::endl;

    std::cout << std::setw(18) << "error_page:";
    printMap(server.error_page);
    std::cout << std::endl;

    std::cout << RED << "------------------------------------------------" << RESET << std::endl;

    for (size_t i = 0; i < server.locations.size(); ++i)
        printLocation(server.locations[i], i);
}

void ConfigParser::test_print()
{
    printGlobalConfig();

    for (size_t i = 0; i < servers.size(); ++i)
        printServerConfig(servers[i], i);
}
