#include "ConfigServer.hpp"

// Canonical form ConfigRedirections
ConfigRedirections::ConfigRedirections()
{
    this->returnCode = 0;
}

ConfigRedirections::ConfigRedirections( const ConfigRedirections& original )
{
    *this = original;
}

ConfigRedirections& ConfigRedirections::operator=( const ConfigRedirections& rhs )
{
    if (this != &rhs)
    {
        this->returnCode = rhs.returnCode;
        this->returnUrl = rhs.returnUrl;
    }
    return (*this);
}

ConfigRedirections::~ConfigRedirections()
{

}

// Canonical form ErrorPages
ErrorPages::ErrorPages()
{

}

ErrorPages::ErrorPages( const ErrorPages& original )
{
    *this = original;
}

ErrorPages& ErrorPages::operator=( const ErrorPages& rhs )
{
    if (this != &rhs)
    {
        this->errorCodes = rhs.errorCodes;
        this->errorPath = rhs.errorPath;
    }
    return (*this);
}

ErrorPages::~ErrorPages()
{

}

// Canonical form ConfigLocation

ConfigLocation::ConfigLocation()
{
    this->autoindex = false;
    this->isAutoindexDefined = false;
    //sentinel values (valores centinela) for avoiding repetitions in these var
    this->locationRoot = "";
    this->indexFile = "";
    this->uploadStore = "";
}

ConfigLocation::ConfigLocation( const ConfigLocation& original )
{
    *this = original;
}

ConfigLocation& ConfigLocation::operator=( const ConfigLocation& rhs )
{
    if (this != &rhs)
    {
        this->path = rhs.path;
        this->locationRoot = rhs.locationRoot;
        this->autoindex = rhs.autoindex;
        this->isAutoindexDefined = rhs.isAutoindexDefined;
        this->indexFile = rhs.indexFile;
        this->allowedMethods = rhs.allowedMethods;
        this->returnRedirections = rhs.returnRedirections;
        this->cgiPath = rhs.cgiPath;
        this->cgiExtension = rhs.cgiExtension;
        this->uploadStore = rhs.uploadStore;
    }
    return (*this);
}

ConfigLocation::~ConfigLocation()
{

}

// Canonical form ConfigServer
ConfigServer::ConfigServer()
{
    //sentinel values (valores centinela) for avoiding repetitions in these var
    this->port = -1;
    this->host = "";
    this->root = "";
    this->indexFile = "";
    this->clientMaxBodySize = 0;
    this->autoindex = false;
    this->isAutoindexDefined = false;
}
/*
ConfigServer::ConfigServer()
{
    // NGINX default configuration
    this->port = 80;
    this->host = "127.0.0.1";
    this->root = "/var/www/html"; // O empty ""
    this->indexFile = "index.html";
    this->clientMaxBodySize = 1000000; // 1 MB
}
*/

ConfigServer::ConfigServer( const ConfigServer& original )
{
    *this = original;
}

ConfigServer& ConfigServer::operator=( const ConfigServer& rhs )
{
    if (this != &rhs)
    {
        this->port = rhs.port;
        this->serverNames = rhs.serverNames;
        this->host = rhs.host;
        this->root = rhs.root;
        this->indexFile = rhs.indexFile;
        this->clientMaxBodySize = rhs.clientMaxBodySize;
        this->errorPages = rhs.errorPages;
        this->locations = rhs.locations;
        this->autoindex = rhs.autoindex;
        this->isAutoindexDefined = rhs.isAutoindexDefined;
    }
    return(*this);
}

ConfigServer::~ConfigServer()
{
    
}