
#include "../../include/configParser.hpp"
#include "../../include/utils.hpp"
#include <set>
#include <stdexcept>
#include <arpa/inet.h>

void ConfigParser::setServerDirective(const std::string& key, const std::string& value, Type t, ServerConfig& serv)
{
    if (!validateType(t, value))
        throw std::runtime_error("Invalid value for directive: " + key + " inside Server Block");
    if (key == "listen")
    {
        if (isPort(value))
        {
                serv.listen_port.push_back(ListenPort{static_cast<uint16_t>(std::stoi(value)), INADDR_ANY});
        }
        else
        {
            size_t colon = value.find(':');
            if (colon == std::string::npos)
                throw std::runtime_error("Invalid listen directive: " + value);
            std::string port_str = value.substr(colon + 1);
            int parsed_port = 0;
            try
            {
                parsed_port = std::stoi(port_str);
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("Invalid listen port: " + port_str);
            }
            uint16_t port = static_cast<uint16_t>(parsed_port);
            uint32_t ip = parseIPv4(value.substr(0, colon));
            serv.listen_port.push_back(ListenPort{port, ip});
        }
    }
    else if (key == "root")
        serv.root = resolveConfigPath(value);
    else if (key == "client_max_body_size")
        serv.client_max_body_size = parseSize(value);
    else if (key == "timeout")
    {
        size_t duration = parseTime(value);
        if (duration < 1000)
            duration = 1000;
        serv.timeout = static_cast<int>(duration / 1000);
    }
    else if (key == "cgi_timeout")
        serv.cgi_timeout = parseTime(value);
    else if (key == "cgi_idle_timeout")
        serv.cgi_idle_timeout = parseTime(value);
}

void ConfigParser::setServerDirective(const std::string& key, const std::vector<std::string>& value, Type t, ServerConfig& serv)
{
    if (!validateType(t, value))
        throw std::runtime_error("Invalid value for directive: " + key);
    if (key == "index")
    {
        serv.index.clear();
        serv.index = value;
    }
    else if (key == "server_name")
    {
        serv.server_name.clear();
        serv.server_name = value;
    }
    else if (key == "error_page")
    {
        if (!serv.error_pages_customized)
        {
            serv.error_page.clear();
            serv.error_pages_customized = true;
        }
        const std::string& path = value.back();
        for (size_t i = 0; i < value.size() - 1; i++)
        {
            int code = 0;
            try
            {
                code = std::stoi(value[i]);
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("Invalid error_page code: " + value[i]);
            }
            if (serv.error_page.count(code))
            {
                throw std::runtime_error("Duplicate error_page entry for code: " + value[i]);
            }
            serv.error_page[code] = path;
        }
    }
}

void ConfigParser::parseServer(const std::vector<std::string>& tokens, size_t& i)
{
    i++;
    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("ConfigSyntaxError: expected '{' after 'server'");
    i++;

    ServerConfig serv{};
    std::set<std::string> duplicateCheck;

    while (i < tokens.size() && tokens[i] != "}")
    {
        const std::string& key = tokens[i]; 

        if (duplicateCheck.count(key) > 0 && key != "location" && key != "error_page")
            throw std::runtime_error("Duplicate directive: " + key);
        duplicateCheck.insert(key); 

        if (i + 1 >= tokens.size())
            throw std::runtime_error("Missing value for directive: " + key);
        if (key == "location")
            parseLocation(tokens, i, serv);
        else if (grammar.at(SERVER).find(key) != grammar.at(SERVER).end())
        {
            Type t = grammar.at(SERVER).at(key);

            if (t == DOMAIN|| t == FILENAME || t == MAP)
            {
                std::vector<std::string> values;
                i++;
                while (i < tokens.size() && tokens[i] != ";")
                {
                    values.push_back(tokens[i]);
                    i++;
                }
                if (i >= tokens.size() || tokens[i] != ";")
                    throw std::runtime_error("Syntax error: Missing ';'");
                setServerDirective(key, values, t, serv);
                i++;
            }
            else 
            {
                const std::string& value = tokens[i + 1];
                if (i + 2 >= tokens.size() || tokens[i + 2] != ";")
                    throw std::runtime_error("Syntax error: Missing ';'");
                setServerDirective(key, value, t, serv);
                i += 3;
            }
        }
        else
            throw std::runtime_error("Unknown directive in server block: " + key);
    }
    
    if (i >= tokens.size() || tokens[i] != "}")
        throw std::runtime_error("Unclosed server block");
    i++;
    if (serv.listen_port.empty())
        throw std::runtime_error("Missing required 'listen' directive in server block");
    if (serv.root.empty())
        throw std::runtime_error("Missing required root for server");
    if (!isDirectory(serv.root))
        throw std::runtime_error("Server root must be a directory");

    servers.push_back(serv);
}
