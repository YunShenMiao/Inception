#include <iostream>
#include <string>
#include <map>
#include "../../include/httpRequest.hpp"
#define RED "\033[31m"
#define RESET "\033[0m"

std::string testRequest =
    "GET /hello/world HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: CustomClient/1.0\r\n"
    "Accept: text/html, application/json\r\n"
    "X-Test-Header:   \r\n"
    "\r\n";

int main()
{
    {
        std::cout << "--- Testing valid request ---\n";

        HttpRequest req;
        ParseState st = req.parseRequest(testRequest.c_str(), testRequest.size());

        if (st == ParseState::COMPLETE)
            req.printRequest();
        else
            std::cerr << RED << "Parser did not finish: state=" << (int)st << RESET << "\n";
    }

    std::cout << "\n-- Now testing malformed requests --\n";

    //missing header colon
    std::string bad1 =
        "GET / HTTP/1.1\r\n"
        "Bad Header Missi408 Request Timeoutng Colon\r\n"
        "\r\n";

    {
        HttpRequest r;
        auto st = r.parseRequest(bad1.c_str(), bad1.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR: " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect malformed header\n" << RESET;
    }

    //invalid space
    std::string bad2 =
        "GET / HTTP/1.1\r\n"
        "Host : example.com\r\n"
        "\r\n";

    {
        HttpRequest r;
        auto st = r.parseRequest(bad2.c_str(), bad2.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR: " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect malformed header\n" << RESET;
    }

    //invalid method
    std::string bad3 =
        "G ET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    {
        HttpRequest r;
        auto st = r.parseRequest(bad3.c_str(), bad3.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR: " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect malformed header\n" << RESET;
    }

    // Missing HTTP version
    std::string bad4 =
    "GET /path\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad4.c_str(), bad4.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (missing HTTP version): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect missing HTTP version\n" << RESET;
}

// Invalid HTTP version
    std::string bad5 =
    "GET / HTTP/2.0\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad5.c_str(), bad5.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (invalid HTTP version): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect invalid HTTP version\n" << RESET;
    }

// Missing URI
    std::string bad6 =
    "GET  HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad6.c_str(), bad6.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (missing URI): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect missing URI\n" << RESET;
    }

    // Empty request line
    std::string bad7 =
    "\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad7.c_str(), bad7.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (empty request line): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect empty request line\n" << RESET;
    }

    // Missing \r\n after request line
    std::string bad8 =
    "GET / HTTP/1.1\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad8.c_str(), bad8.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (missing \\r\\n): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect missing \\r\\n\n" << RESET;
    }

    // Header with no value (just colon)
    std::string bad9 =
    "GET / HTTP/1.1\r\n"
    "Host:\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad9.c_str(), bad9.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (header with no value): " << std::endl;
            r.printError();
        }
        else
            std::cerr << "[FAIL] Did not detect header with no value\n";
    }

    // Empty header name
    std::string bad10 =
    "GET / HTTP/1.1\r\n"
    ": value\r\n"
    "\r\n";
    {   
        HttpRequest r;
        auto st = r.parseRequest(bad10.c_str(), bad10.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (empty header name): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect empty header name\n" << RESET;
    }

    // Multiple spaces in request line
    std::string bad11 =
    "GET  /path  HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad11.c_str(), bad11.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (multiple spaces): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect multiple spaces in request line\n" << RESET;
}

    // Invalid characters in method
    std::string bad12 =
    "GET@BAD / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad12.c_str(), bad12.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (invalid method chars): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect invalid characters in method\n" << RESET;
}

    // Missing Host header (HTTP/1.1 requirement)
    std::string bad13 =
    "GET / HTTP/1.1\r\n"
    "User-Agent: Test\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad13.c_str(), bad13.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (missing Host header): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect missing Host header\n" << RESET;
    }

    // Header name with space
    std::string bad14 =
    "GET / HTTP/1.1\r\n"
    "Bad Header: value\r\n"
    "\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad14.c_str(), bad14.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported ERROR (space in header name): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect space in header name\n" << RESET;
}
//
    // No final \r\n\r\n
    std::string bad15 =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n";
    {
        HttpRequest r;
        auto st = r.parseRequest(bad15.c_str(), bad15.size());
        if (st == ParseState::ERROR)
        {
            std::cout << "Parser correctly reported INCOMPLETE (no final \\r\\n\\r\\n): " << std::endl;
            r.printError();
        }
        else
            std::cerr << RED << "[FAIL] Did not detect incomplete request\n" << RESET;
    }

    return 0;
}
