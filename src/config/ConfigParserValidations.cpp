#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr
#include <sys/stat.h>  // stat()
#include <stdexcept>

// NGINX: 1-1023 (priviliged ports)
void ConfigParser::_warnPrivilegedPorts()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].port > 0 && _servers[i].port < 1024)
        {
            std::cerr << "Warning: Server configured on privileged port " << _servers[i].port << std::endl;
        }
    }
}
// NGINX: validate root /var/www/html existence
void ConfigParser::_validateDirectoriesExist()
{
    struct stat info;

    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (!_servers[i].root.empty() && stat(_servers[i].root.c_str(), &info) != 0)
        {
            throw std::runtime_error("Error: Server root directory '" + _servers[i].root + "' does not exist.");
        }
        
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];

            if (!loc.locationRoot.empty() && stat(loc.locationRoot.c_str(), &info) != 0)
            {
                throw std::runtime_error("Error: Location root '" + loc.locationRoot + "' does not exist.");
            }
            
            if (!loc.uploadStore.empty() && stat(loc.uploadStore.c_str(), &info) != 0)
            {
                throw std::runtime_error("Error: Upload store directory '" + loc.uploadStore + "' does not exist.");
            }
        }
    }
}
// NGINX: Valid redirections: 301, 302, 303, 307 or 308
void ConfigParser::_validateRedirections()
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
                        std::cerr << "Error: Redirect to URL " << redir.returnUrl << " must use 301, 302, 303, 307 or 308" << std::endl;
                        throw std::runtime_error("Configuration validation failed");
                    }
                }
            }
        }
    }
}
// NGINX: avoid access to other routes not allowed
void ConfigParser::_validatePathTraversal()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].root.find("../") != std::string::npos)
        {
            throw std::runtime_error("Error: Path traversal attempt detected in server root.");
        }
        
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];

            if (loc.path.find("../") != std::string::npos || 
                loc.locationRoot.find("../") != std::string::npos || 
                loc.uploadStore.find("../") != std::string::npos)
            {
                throw std::runtime_error("Error: Path traversal attempt (../) detected in location configuration.");
            }
        }
    }
}

// Función auxiliar estática/privada para verificar si un path apunta a la raíz del sistema
static bool isSystemRootPath(const std::string& path)
{
    if (path.empty())
        return false;

    // Si solo contiene una o más barras consecutivas ("/", "///", etc.)
    size_t firstNonSlash = path.find_first_not_of('/');
    if (firstNonSlash == std::string::npos)
        return true;

    return false;
}

void ConfigParser::_validateForbiddenRoots()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        // 1. Validar el root global del server (si está configurado)
        if (isSystemRootPath(_servers[i].root))
        {
            throw std::runtime_error("Config Error: Server root cannot point to system root ('/')");
        }

        // 2. Validar cada location
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            const ConfigLocation& loc = _servers[i].locations[j];

            // Validar locationRoot
            if (isSystemRootPath(loc.locationRoot))
            {
                throw std::runtime_error("Config Error: locationRoot cannot point to system root ('/') in location '" + loc.path + "'");
            }

            // Validar uploadStore por si alguien intenta subir archivos directamente a /
            if (isSystemRootPath(loc.uploadStore))
            {
                throw std::runtime_error("Config Error: uploadStore cannot point to system root ('/') in location '" + loc.path + "'");
            }
        }
    }
}

void ConfigParser::_validateSemantic()
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
            _servers[i].root = "www"; // O la carpeta pública que uséis
        if (_servers[i].indexFile == "")
            _servers[i].indexFile = "index.html";
        
        if (_servers[i].port < 1 || _servers[i].port > 65535)
        {
            std::cerr << "Error: Port " << _servers[i].port << " is out of valid range (1-65535)." << std::endl;
            throw std::runtime_error("Configuration validation failed");
        }

        for (size_t j = 0; j < _servers[i].errorPages.size(); ++j)
        {
            for (size_t k = 0; k < _servers[i].errorPages[j].errorCodes.size(); ++k)
            {
                int code = _servers[i].errorPages[j].errorCodes[k];
                if (code < 400 || code > 599)
                {
                    std::cerr << "Error: Invalid error page code " << code << " (must be 400-599)." << std::endl; // cerr and throw because c++98. Issues with int for print!
                    throw std::runtime_error("Configuration validation failed");
                }
            }
        }

        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& location = _servers[i].locations[j];

            if (location.cgiPath.size() != location.cgiExtension.size())
            {
                throw std::runtime_error("Error: Each cgi extension must have exactly one path");
            }
            
            for (size_t k = j + 1; k < _servers[i].locations.size(); ++k)
            {
                if (location.path == _servers[i].locations[k].path)
                {
                    throw std::runtime_error("Error: Duplicate location path " + location.path + " in the same server");
                }
            }

            if (location.allowedMethods.empty())
            {
                location.allowedMethods.push_back("GET");
            }
            else
            {
                for (size_t k = 0; k < location.allowedMethods.size(); ++k)
                {
                    const std::string& method = location.allowedMethods[k];
                    if (method != "GET" && method != "POST" && method != "DELETE")
                    {
                        throw std::runtime_error("Error: Invalid HTTP method " + method + ". Allowed: GET, POST, DELETE");
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
                    throw std::runtime_error("Configuration validation failed");
                }
                for (size_t n1 = 0; n1 < _servers[i].serverNames.size(); ++n1)
                {
                    for (size_t n2 = 0; n2 < _servers[j].serverNames.size(); ++n2)
                    {
                        if (_servers[i].serverNames[n1] == _servers[j].serverNames[n2])
                        {
                            throw std::runtime_error("Error: Server name " + _servers[i].serverNames[n1] + " is duplicated on the same host and port.");
                        }
                    }
                }
            }
        }
    }
    
    _validatePathTraversal();
    _validateForbiddenRoots();
    _validateRedirections();
    _validateDirectoriesExist();
    _warnPrivilegedPorts();
}