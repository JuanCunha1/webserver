#include "ConfigParser.hpp"
#include <iostream>    // std::cerr
#include <sys/stat.h>  // stat()

// NGINX: Warning program must execute with sudo if 1-1023 (priviliged ports)
void ConfigParser::_warnPrivilegedPorts()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port > 0 && _servers[i].port < 1024)
        {
            std::cerr << "Warning: Server configured on privileged port " << _servers[i].port << ". Requires root/sudo execution." << std::endl;
        }
    }
}

// NGINX: validate root /var/www/html existence
bool ConfigParser::_validateDirectoriesExist()
{
    struct stat info;

    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (!_servers[i].root.empty() && stat(_servers[i].root.c_str(), &info) != 0)
        {
            std::cerr << "Error: Server root directory '" << _servers[i].root << "' does not exist." << std::endl;
            return (false);
        }

        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];
            
            if (!loc.locationRoot.empty() && stat(loc.locationRoot.c_str(), &info) != 0)
            {
                std::cerr << "Error: Location root '" << loc.locationRoot << "' does not exist." << std::endl;
                return (false);
            }
            if (!loc.uploadStore.empty() && stat(loc.uploadStore.c_str(), &info) != 0)
            {
                std::cerr << "Error: Upload store directory '" << loc.uploadStore << "' does not exist." << std::endl;
                return (false);
            }
        }
    }
    return (true);
}

// NGINX: Valid redirections: 301, 302, 303, 307 or 308
bool ConfigParser::_validateRedirections()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];
            for (size_t k = 0; k < loc.returnRedirections.size(); ++k)
            {
                ConfigRedirections& redir = loc.returnRedirections[k];
                if (!redir.returnUrl.empty())
                {
                    int c = redir.returnCode;
                    if (c != 301 && c != 302 && c != 303 && c != 307 && c != 308)
                    {
                        std::cerr << "Error: Redirect to URL " << redir.returnUrl << " must use 301, 302, 303, 307 or 308 return code" << std::endl;
                        return (false);
                    }
                }
            }
        }
    }
    return (true);
}

// NGINX: avoid access to other routes not allowed
bool ConfigParser::_validatePathTraversal()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].root.find("../") != std::string::npos)
        {
            std::cerr << "Error: Path traversal attempt detected in server root." << std::endl;
            return (false);
        }
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];

            if (loc.path.find("../") != std::string::npos || 
                loc.locationRoot.find("../") != std::string::npos || 
                loc.uploadStore.find("../") != std::string::npos)
            {
                std::cerr << "Error: Path traversal attempt (../) detected in location configuration." << std::endl;
                return (false);
            }
        }
    }
    return (true);
}

bool ConfigParser::_validateSemantic()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        // Default directives according to NGINX
        if (_servers[i].port == -1)
            _servers[i].port = 80;
        if (_servers[i].host == "")
            _servers[i].host = "127.0.0.1";
        if (_servers[i].clientMaxBodySize == 0)
            _servers[i].clientMaxBodySize = 1000000; // 1 MB
        if (_servers[i].root == "")
            _servers[i].root = "/var/www/html"; // O la carpeta pública que uséis
        if (_servers[i].indexFile == "")
            _servers[i].indexFile = "index.html";
        if (_servers[i].port < 1 || _servers[i].port > 65535)
        {
            std::cerr << "Error: Port " << _servers[i].port << " is out of valid range (1-65535)." << std::endl;
            return (false);
        }
        for (size_t j = 0; j < _servers[i].errorPages.size(); ++j)
        {
            for (size_t k = 0; k < _servers[i].errorPages[j].errorCodes.size(); ++k)
            {
                int code = _servers[i].errorPages[j].errorCodes[k];
                if (code < 400 || code > 599) // 400 - 499 --> Client errors.    500 - 599 --> Server errors
                {
                    std::cerr << "Error: Invalid error page code " << code << " (must be 400-599)." << std::endl;
                    return (false);
                }
            }
        }
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& location = _servers[i].locations[j]; // Use of & for modify vector if empty

            if (location.cgiPath.size() != location.cgiExtension.size())
            {
                std::cerr << "Error: Each cgi extension must have exactly one path" << std::endl;
                return (false);
            }
            for (size_t k = j + 1; k < _servers[i].locations.size(); ++k)
            {
                if (location.path == _servers[i].locations[k].path)
                {
                    std::cerr << "Error: Duplicate location path " << location.path << " in the same server" << std::endl;
                    return (false);
                }
            }
            if (location.allowedMethods.empty())
            {
                location.allowedMethods.push_back("GET"); // Security measure
            }
            else
            {
                for (size_t k = 0; k < location.allowedMethods.size(); ++k)
                {
                    const std::string& method = location.allowedMethods[k];
                    if (method != "GET" && method != "POST" && method != "DELETE")
                    {
                        std::cerr << "Error: Invalid HTTP method " << method << ". Allowed: GET, POST, DELETE" << std::endl;
                        return (false);
                    }
                }
            }
        }
    }
    // version HTTP 1.1 --> If both share the same port and IP, they must have distinct server_names. If either lacks a defined server_name, there is ambiguity.
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        for (size_t j = i + 1; j < _servers.size(); ++j)
        {
            if (_servers[i].port == _servers[j].port && _servers[i].host == _servers[j].host)
            {
                if (_servers[i].serverNames.empty() || _servers[j].serverNames.empty())
                {
                    std::cerr << "Error: Servers sharing host " << _servers[i].host << " and port " << _servers[i].port << " must have different server_names defined" << std::endl;
                    return (false);
                }
                for (size_t n1 = 0; n1 < _servers[i].serverNames.size(); ++n1)
                {
                    for (size_t n2 = 0; n2 < _servers[j].serverNames.size(); ++n2)
                    {
                        if (_servers[i].serverNames[n1] == _servers[j].serverNames[n2])
                        {
                            std::cerr << "Error: Server name " << _servers[i].serverNames[n1] << " is duplicated on the same host and port." << std::endl;
                            return (false);
                        }
                    }
                }
            }
        }
    }
    if (!_validatePathTraversal())
        return (false);
    if (!_validateRedirections())
        return (false);
    if (!_validateDirectoriesExist())
        return (false);
        
    _warnPrivilegedPorts();
    return (true);
}