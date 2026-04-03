#include <ostream>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <arpa/inet.h>

#include "../../include/configParser.hpp"
#include "../../include/utils.hpp"

const std::map<Block, std::map<std::string, Type>> ConfigParser::grammar =
{
    {GLOBAL, 
        {
            {"error_log", PATH},
            {"pid", PATH}
        }
    },
    {SERVER, 
        {
            {"listen", PORT},
            {"root", PATH},
            {"index", FILENAME},
            {"server_name", DOMAIN},
            {"error_page", MAP},
            {"client_max_body_size", SIZE},
            {"timeout", TIME},
            {"cgi_timeout", TIME},
            {"cgi_idle_timeout", TIME},
            {"location", BLOCK}
        }
    },
    {LOCATION, 
        {
            {"root", PATH},
            {"autoindex", BOOLEAN},
            {"botdetect", YESNO},
            {"index", FILENAME},
            {"allow_methods", METH},
            {"upload_path", PATH},
            {"cgi_extension", CGI_EXT},
            {"cgi_path", PATH},
            {"client_max_body_size", SIZE},
            {"return", REDIRECT}
        }
    }
};

bool validateType(Type t, const std::vector<std::string>& value)
{
    if (value.empty())
        return false;
    switch(t)
    {
        case METH:
            for (size_t i = 0; i < value.size(); i++)
            {
                if (!isMethod(value[i]))
                    return false;
            }
                return true;
        case DOMAIN:
            for (size_t i = 0; i < value.size(); i++)
            {
                if (value[i] == "_")
                    return true;
                if (!isDomainname(value[i]))
                    return false;
            }
                return true;
        case FILENAME:
            for (size_t i = 0; i < value.size(); i++)
            {
                if (!isFilename(value[i]))
                    return false;
            }
                return true;
        case CGI_EXT:
            for (size_t i = 0; i < value.size(); i++)
            {
                if (value[i] != ".py" && value[i] != ".php" && value[i] != ".sh")
                    return false;
            }
                return true;
        case MAP:
            if (value.size() != 2)
                return false;
            for (size_t i = 0; i < value.size() - 1; i++)
            {
                if (!isErrorCode(value[0]))
                    return false;
                if (value[1].empty() || value[1].find("..") != std::string::npos || value[1][0] != '/')
                    return false;
            }
            return isPath(value.back());
        case REDIRECT:
            return isRedirect(value);
        default:
            return false;
    }
}

bool validateType(Type t, const std::string& value)
{
    switch (t) 
    {
        case PORT:
            return isListen(value);
        case PATH:
            return isPath(value);
        case BOOLEAN:
            return isBoolean(value);
        case SIZE:
            return isSize(value);
        case TIME:
            return isTime(value);
        case YESNO:
            return isYesNo(value);
        default:
            return false;
    }
}

bool isFilename(const std::string& str)
{
    if (str.empty() || str.find_first_of("/\0") != std::string::npos
        || str.length() > 255)
        return false;
    return true;
}

bool isErrorCode(const std::string& str)
{
    if (!isNumber(str))
        return false;
    try
    {
        int code = std::stoi(str);
        return code >= 400 && code <= 599;
    }
    catch (const std::exception &e)
    {
        return false;
    }
}

bool isPath(const std::string& str)
{
    if (str.empty())
        return false;
    if (str.find('\0') != std::string::npos)
        return false;
    return true;
}

bool isLocationPath(const std::string& str)
{
    if (str.empty() || str[0] != '/')
    return false;

    if (str.find("..") != std::string::npos)
        return false;
    for (char c : str)
    {
        if (!std::isalnum(c) && c != '/' &&
            c != '-' && c != '.' && c != '_')
            return false;
    }
    return true;
}

bool isSize(const std::string& str)
{
    if (str.empty())
        return false;
    std::string numbers;
    char suffix = '\0';
    if (!std::isdigit(str.back()))
    {
        suffix = std::toupper(str.back());
        if (suffix != 'K' && suffix != 'M' && suffix != 'G')
            return false;
        numbers = str.substr(0, str.size() - 1);
    }
    else
        numbers = str;
    if (!isNumber(numbers))
        return false;
    try
    {
        size_t size = std::stoul(numbers);
        size_t max_size = std::numeric_limits<size_t>::max();
        if (suffix == 'G' && size > max_size / (1024ULL * 1024 * 1024))
            return false;
        if (suffix == 'M' && size > max_size / (1024 *1024))
            return false;   
        if (suffix == 'K' && size > max_size / 1024)
            return false;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

size_t parseSize(const std::string& str)
{
    std::string numbers;
    char suffix = '\0';
    
    if (!std::isdigit(str.back()))
    {
        suffix = std::toupper(str.back());
        numbers = str.substr(0, str.size() - 1);
    }
    else
        numbers = str;

    size_t size = 0;
    try
    {
        size = std::stoul(numbers);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("Invalid size value: " + str);
    }
    switch (suffix)
    {
        case 'K': return size * 1024;
        case 'M': return size * 1024 * 1024;
        case 'G': return size * 1024 * 1024 * 1024;
        default: return size;
    }
}

bool isBoolean(const std::string& str)
{
    return str == "on" || str == "off";
}

bool isYesNo(const std::string& str)
{
    return str == "yes" || str == "no";
}

bool isTime(const std::string& str)
{
    if (str.empty())
        return false;
    std::string numbers = str;
    std::string suffix;
    if (str.size() > 2)
    {
        std::string tail = str.substr(str.size() - 2);
        if (tail == "ms" || tail == "MS")
        {
            suffix = "ms";
            numbers = str.substr(0, str.size() - 2);
        }
    }
    if (suffix.empty())
    {
        char last = str.back();
        if (last == 's' || last == 'S' || last == 'm' || last == 'M')
        {
            suffix = std::string(1, static_cast<char>(std::tolower(last)));
            numbers = str.substr(0, str.size() - 1);
        }
    }
    if (numbers.empty() || !isNumber(numbers))
        return false;
    try
    {
        size_t base = std::stoul(numbers);
        size_t multiplier = 1000; // seconds default
        if (suffix == "ms")
            multiplier = 1;
        else if (suffix == "m")
            multiplier = 60 * 1000;
        if (base == 0)
            return false;
        size_t max_size = std::numeric_limits<size_t>::max() / multiplier;
        if (base > max_size)
            return false;
        // 24h absolute guard (in ms)
        if (base * multiplier > 86400ULL * 1000ULL)
            return false;
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool isRedirect(const std::vector<std::string>& values)
{
    if (values.size() > 2 || values.size() < 1)
        return false;
    if (!isNumber(values[0]))
        return false;
    try
    {
        int code = std::stoi(values[0]);
        if (code == 400 || code == 403 || code == 404 || code == 405
            || code == 500 || code == 501)
            return true;
        if (values.size() == 2 && (isPath(values[1]) || isUrl(values[1])) &&
            (code == 301 || code == 302 || code == 303 || code == 307 || code == 308))
            return true;
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool isUrl(const std::string& str)
{
    if (str.find("http://") == 0 || str.find("https://") == 0)
        return str.length() > 7;
    return false;
}

uint32_t parseIPv4(const std::string& ip_str)
{
    uint32_t ip_bin;
    int ret = inet_pton(AF_INET, ip_str.c_str(), &ip_bin);
    if (ret != 1)
        throw std::runtime_error("Invalid IPv4 address: " + ip_str);
    return ip_bin;
}

size_t parseTime(const std::string& str)
{
    if (!isTime(str))
        throw std::runtime_error("Invalid time value: " + str);

    std::string numbers = str;
    std::string suffix;
    if (str.size() > 2)
    {
        std::string tail = str.substr(str.size() - 2);
        if (tail == "ms" || tail == "MS")
        {
            suffix = "ms";
            numbers = str.substr(0, str.size() - 2);
        }
    }
    if (suffix.empty())
    {
        char last = str.back();
        if (last == 's' || last == 'S' || last == 'm' || last == 'M')
        {
            suffix = std::string(1, static_cast<char>(std::tolower(last)));
            numbers = str.substr(0, str.size() - 1);
        }
    }

    size_t base = 0;
    try
    {
        base = std::stoul(numbers);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("Invalid time value: " + str);
    }
    size_t multiplier = 1000;
    if (suffix == "ms")
        multiplier = 1;
    else if (suffix == "m")
        multiplier = 60 * 1000;

    return base * multiplier;
}
