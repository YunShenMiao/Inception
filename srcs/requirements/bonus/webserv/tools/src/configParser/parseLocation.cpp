#include "../../include/configParser.hpp"
#include "../../include/utils.hpp"
#include <set>
#include <stdexcept>

void ConfigParser::setLocationDefaults(LocationConfig& loc, ServerConfig& serv)
{
    loc.client_max_body_size = serv.client_max_body_size;
    loc.root = serv.root;
    loc.index = serv.index;
    loc.allow_methods = {"GET", "POST", "DELETE"};
    loc.botdetect = true;
}

void ConfigParser::setLocDirective(const std::string& key, const std::string& value, Type t, LocationConfig& loc)
{
    if (!validateType(t, value))
        throw std::runtime_error("Invalid value for directive: " + key + " inside Location Block");
    if (key == "root")
        loc.root = resolveConfigPath(value);
    else if (key == "autoindex")
    {
        if (value == "on")
            loc.autoindex = 1;
    }
    else if (key == "client_max_body_size")
        loc.client_max_body_size = parseSize(value);
    else if (key == "cgi_path")
        loc.cgi_path = resolveConfigPath(value);
    else if (key == "upload_path")
        loc.upload_path = resolveConfigPath(value);
    else if (key == "botdetect")
        loc.botdetect = (value == "yes");
}

void ConfigParser::setLocDirective(const std::string& key, const std::vector<std::string>& value, Type t, LocationConfig& loc)
{
    if (!validateType(t, value))
        throw std::runtime_error("Invalid value for directive: " + key);
    if (key == "index")
    {
        loc.index.clear();
        loc.index = value;
    }
    else if (key == "allow_methods")
    {
        loc.allow_methods.clear();
        loc.allow_methods = value;
    }
    else if (key == "cgi_extension")
        loc.cgi_extension = value;
    else if (key == "return")
        loc.return_ = value;
}

void ConfigParser::parseLocation(const std::vector<std::string>& tokens, size_t& i, ServerConfig& serv)
{
    i++;
    if (i >= tokens.size() || !isLocationPath(tokens[i]))
        throw std::runtime_error("invalid path for Location Block");

    LocationConfig loc;
    loc.path = tokens[i];
    i++;

    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("ConfigSyntaxError: expected '{' after 'server'");
    i++;
    setLocationDefaults(loc, serv);

    std::set<std::string> duplicateCheck;

    while (i < tokens.size() && tokens[i] != "}")
    {
        const std::string& key = tokens[i]; 
        if (duplicateCheck.count(key) > 0)
            throw std::runtime_error("Duplicate directive: " + key);
        duplicateCheck.insert(key); 

        if (i + 1 >= tokens.size())
            throw std::runtime_error("Missing value for directive: " + key);
        else if (grammar.at(LOCATION).find(key) != grammar.at(LOCATION).end())
        {
            Type t = grammar.at(LOCATION).at(key);
            i++;

            if (t == FILENAME || t == METH || t == CGI_EXT || t == REDIRECT)
            {
                std::vector<std::string> values;
                while (i < tokens.size() && tokens[i] != ";")
                {
                    values.push_back(tokens[i]);
                    i++;
                }
                if (i >= tokens.size() || tokens[i] != ";")
                    throw std::runtime_error("Syntax error: Missing ';'");
                setLocDirective(key, values, t, loc);
                i++;
            }
            else
            {
                const std::string& value = tokens[i];
                if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                    throw std::runtime_error("Syntax error: Missing ';'");
                if (!validateType(t, value))
                    throw std::runtime_error("Invalid type for directive: " + key + " inside Location Block");
                setLocDirective(key, value, t, loc);
                i += 2;
            }
        }
        else
            throw std::runtime_error("Unknown directive in Location Block: " + key);
    }
     if (i >= tokens.size() || tokens[i] != "}")
        throw std::runtime_error("Unclosed location block");
    i++;
    if (!loc.root.empty())
    {
        if (!isDirectory(loc.root))
            throw std::runtime_error("Location root must be a directory");
    }
        if (!loc.upload_path.empty())
    {
        if (!isDirectory(loc.upload_path))
            throw std::runtime_error("Upload path must be a directory");
    }
    if (!loc.return_.empty())
    {
        if (loc.return_.size() > 1 && !loc.return_[1].empty() && loc.path == loc.return_[1])
            throw std::runtime_error("Return loop detected");
    }
    serv.locations.push_back(loc);
}
