#include "../include/configParser.hpp"
#include "../include/server.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        ConfigParser conf = (argc > 1) ? ConfigParser(argv[1]) : ConfigParser();
        conf.parse();

        Server server(conf);
        server.init();
        server.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}