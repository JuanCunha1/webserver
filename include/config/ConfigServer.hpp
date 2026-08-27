#pragma once

#include <string>
#include <vector>

struct ConfigRedirections
{
    ConfigRedirections();
    ConfigRedirections( const ConfigRedirections& original );
    ConfigRedirections& operator=( const ConfigRedirections& rhs );
    ~ConfigRedirections();

    // location {}
    int         returnCode;
    std::string returnUrl;
};

struct ErrorPages
{
    ErrorPages();
    ErrorPages( const ErrorPages& original );
    ErrorPages& operator=( const ErrorPages& rhs );
    ~ErrorPages();

    // server {}
    std::vector<int>    errorCodes;
    std::string         errorPath;
};

struct ConfigLocation
{
    ConfigLocation();
    ConfigLocation( const ConfigLocation& original );
    ConfigLocation& operator=( const ConfigLocation& rhs );
    ~ConfigLocation();

    // location {}
    std::string                     path;
    std::string                     locationRoot;
    bool                            autoindex; // on (true)/off (false)
    std::string                     indexFile;
    std::vector<std::string>        allowedMethods;
    std::vector<ConfigRedirections> returnRedirections;
    std::vector<std::string>        cgiPath;
    std::vector<std::string>        cgiExtension;
};

struct ConfigServer // struct public by default. class private by default
{
    public:
        ConfigServer();
        ConfigServer( const ConfigServer& original );
        ConfigServer& operator=( const ConfigServer& rhs );
        ~ConfigServer();

        // server {}
        int                         port;
        std::vector<std::string>    serverNames;
        std::string                 host;
        std::string                 root;
        std::string                 indexFile;
        unsigned long               clientMaxBodySize;
        std::vector<ErrorPages>     errorPages;
        // location {}
        std::vector<ConfigLocation> locations;
};