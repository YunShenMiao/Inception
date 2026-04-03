
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "../../include/configParser.hpp"

/***********************************************************/
/*               CONSTRUCTOR & DESTRUCTOR                  */
/***********************************************************/

ConfigParser::ConfigParser(): ConfigPath("config/default.conf") {}

ConfigParser::ConfigParser(std::string path): ConfigPath(path) {}

ConfigParser::ConfigParser(const ConfigParser& other): ConfigPath(other.ConfigPath) {}

ConfigParser& ConfigParser::operator=(const ConfigParser& other)
{
    if (this != &other)
        ConfigPath = other.ConfigPath;
    return *this;
}

ConfigParser::~ConfigParser() {}

/***********************************************************/
/*                          GETTER                         */
/***********************************************************/

const GlobalConfig& ConfigParser::getGlobalConfig() const
{
    return global_config;
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
    return servers;
}

/***********************************************************/
/*                    PARSE & TOKENIZE                     */
/***********************************************************/

bool ConfigParser::validLine(std::string str)
{
    str.erase(std::remove_if(str.begin(), str.end(), 
              [](unsigned char c) { return std::isspace(c); }), 
              str.end());
    if (str == "server" || str == "server{" || str == "{" || str == "}")
        return true;
    if (str.find("location") != std::string::npos)
        return true;
    if (str.find(";") != str.size() - 1)
        return false;
    return true;
}

std::vector<std::string> ConfigParser::tokenizeFile()
{
    std::ifstream file(ConfigPath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + ConfigPath);
    
    std::vector<std::string> tokens;
    std::string line;

    while (std::getline(file,line))
    {
        size_t comment = line.find('#');
        if (comment != std::string::npos)
            line = line.substr(0, comment);
        if (!validLine(line))
            throw std::runtime_error("Invalid line syntax: " + line);

        std::istringstream iss(line);
        std::string word;

        while (iss >> word)
        {
            if (!word.empty() && word.back() == ';')
            {
                tokens.push_back(word.substr(0, word.size() - 1));
                tokens.push_back(";");
            }
            else
                tokens.push_back(word);
        }
    }
    return tokens;
}

void ConfigParser::parse()
{
    std::vector<std::string> tokens = tokenizeFile();

    size_t i = 0;

    parseGlobalConfig(tokens, i);

    while (i < tokens.size()) 
    {
        if (tokens[i] == "server")
            parseServer(tokens, i);
        else
            throw std::runtime_error("Unexpected token outside server block: " + tokens[i]);
    }
    if (servers.empty())
        throw std::runtime_error("No server blocks found in config file");

}
