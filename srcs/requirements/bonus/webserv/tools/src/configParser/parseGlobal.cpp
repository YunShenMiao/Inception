#include "../../include/configParser.hpp"
#include "../../include/utils.hpp"

#include <stdexcept>
#include <thread>
#include <set>

void ConfigParser::setDefaultGC()
{
    if (global_config.error_log.empty())
        global_config.error_log = "./logs/error.log";
    if (global_config.pid.empty())
        global_config.pid = "./webserv.pid";
}

void ConfigParser::setGlobalDirective(const std::string& key, const std::string& value)
{
    if (key == "error_log")
        global_config.error_log = resolveConfigPath(value);
    else if (key == "pid")
        global_config.pid = resolveConfigPath(value);
}

void ConfigParser::parseGlobalConfig(const std::vector<std::string>& tokens, size_t& i)
{
    std::set<std::string> duplicateCheck;
    while (i < tokens.size() && tokens[i] != "server") 
    {
        const std::string& key = tokens[i];

        if (duplicateCheck.count(key) > 0)
            throw std::runtime_error("Duplicate directive: " + key);
        duplicateCheck.insert(key); 

        if (i + 1 >= tokens.size())
            throw std::runtime_error("Missing value for global directive: " + key);
        if (i + 2 >= tokens.size() || tokens[i + 2] != ";")
            throw std::runtime_error("Syntax error: Missing ';'");
        const std::string& value = tokens[i + 1];
        if (grammar.at(GLOBAL).find(key) != grammar.at(GLOBAL).end()) 
        {
            Type t = grammar.at(GLOBAL).at(key);
            if (!validateType(t, value))
                throw std::runtime_error("Invalid type for directive: " + key);
            setGlobalDirective(key, value);
        }
        else
            throw std::runtime_error("Invalid directive in global block: " + tokens[i]);
        i += 3;
    }
    setDefaultGC();
}
