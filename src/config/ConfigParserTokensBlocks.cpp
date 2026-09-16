#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr

bool ConfigParser::_duplicateErrorMessage(const std::string& directive)
{
    std::cerr << "Error: '" << directive << "' directive is duplicated." << std::endl;
    return (false);
}

bool ConfigParser::_parseServerBlock(ConfigServer& server)
{
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "{")
    {
        std::cerr << "Error: '{' expected after server" << std::endl;
        return (false);
    }
    ++_tokenIndex;
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != "}")
    {
        if (_tokens[_tokenIndex] == "listen")
        {
            if (server.port != -1)
                return _duplicateErrorMessage("listen");
            if (!_parseNumber(server.port, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "host")
        {
            if (server.host != "")
                return _duplicateErrorMessage("host");
            if (!_parseSingleString(server.host, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "root")
        {
            if (server.root != "")
                return _duplicateErrorMessage("root");
            if (!_parseSingleString(server.root, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
            if (server.indexFile != "")
                return _duplicateErrorMessage("index");
            if (!_parseSingleString(server.indexFile, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "server_name")
        {
            if (!_parseVector(server.serverNames, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "client_max_body_size")
        {
            if (server.clientMaxBodySize != 0)
                return _duplicateErrorMessage("client_max_body_size");
            ++_tokenIndex;
            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
            {
                std::cerr << "Error: 'client_max_body_size' must have a value" << std::endl;
                return (false);
            }
            if (!_parseClientMaxBodySize(server.clientMaxBodySize, _tokens[_tokenIndex]))
                return (false);
            ++_tokenIndex;
            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
            {
                std::cerr << "Error: ';' missing after 'client_max_body_size'" << std::endl;
                return (false);
            }
            ++_tokenIndex;
        }
        else if (_tokens[_tokenIndex] == "error_page")
        {
            ++_tokenIndex;
            std::vector<std::string> args;
            while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != ";")
            {
                args.push_back(_tokens[_tokenIndex]);
                ++_tokenIndex;
            }

            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";" || args.size() < 2)
            {
                std::cerr << "Error: Error page not written correctly" << std::endl;
                return (false);
            }
            ErrorPages ePages;
            ePages.errorPath = args.back();
            for (size_t i = 0; i < args.size() - 1; ++i)
            {
                for (size_t j = 0; j < args[i].length(); ++j)
                {
                    if (!std::isdigit(args[i][j]))
                    {
                        std::cerr << "Error: Invalid error code '" << args[i] << "' in error_page" << std::endl;
                        return (false);
                    }
                }
                int code = std::atoi(args[i].c_str());
                ePages.errorCodes.push_back(code);
            }
            server.errorPages.push_back(ePages);
            ++_tokenIndex;
        }
        else if (_tokens[_tokenIndex] == "location")
        {
            ConfigLocation loc;
            if (!_parseLocationBlock(loc))
                return (false);
            server.locations.push_back(loc);
        }
        else if (_tokens[_tokenIndex] == "autoindex")
        {
            if (server.isAutoindexDefined)
                return _duplicateErrorMessage("autoindex");

            std::string autoindexStr;
            if (!_parseSingleString(autoindexStr, _tokens[_tokenIndex]))
                return (false);
            server.isAutoindexDefined = true;
            if (autoindexStr == "on")
                server.autoindex = true;
            else if (autoindexStr == "off")
                server.autoindex = false;
            else
            {
                std::cerr << "Error: Invalid autoindex value (must be 'on' or 'off')" << std::endl;
                return (false);
            }
        }
        else
        {
            std::cerr << "Error: Unknown " << _tokens[_tokenIndex] << " key in server " << std::endl;
            return (false);
        }
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "}")
    {
        std::cerr << "Error: '}' expected at the end of de server block" << std::endl;
        return (false);
    }
    ++_tokenIndex;
    return (true);
}

bool ConfigParser::_parseLocationBlock(ConfigLocation& location)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == "{" || _tokens[_tokenIndex] == ";")
    {
        std::cerr << "Error: Location path missing" << std::endl;
        return (false);
    }
    location.path = _tokens[_tokenIndex];
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "{")
    {
        std::cerr << "Error: '{' expected after location path" << std::endl;
        return (false);
    }
    ++_tokenIndex;
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != "}")
    {
        if (_tokens[_tokenIndex] == "root")
        {
            if (location.locationRoot != "")
                return _duplicateErrorMessage("root");
            if (!_parseSingleString(location.locationRoot, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
            if (location.indexFile != "")
                return _duplicateErrorMessage("index");
            if (!_parseSingleString(location.indexFile, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "allow_methods") // GET POST DELETE
        {
            if (!_parseVector(location.allowedMethods, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "autoindex")
        {
            std::string autoindexStr;
            if (!_parseSingleString(autoindexStr, _tokens[_tokenIndex]))
                return (false);
            location.isAutoindexDefined = true;
            if (autoindexStr == "on")
                location.autoindex = true;
            else if (autoindexStr == "off")
                location.autoindex = false;
            else
            {
                std::cerr << "Error: Invalid autoindex value (must be 'on' or 'off')" << std::endl;
                return (false);
            }
        }
        else if (_tokens[_tokenIndex] == "return")
        {
            if (!_parseReturn(location.returnRedirections))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "cgi_path")
        {
            if (!_parseVector(location.cgiPath, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "cgi_extension")
        {
            if (!_parseVector(location.cgiExtension, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "upload_store")
        {
            if (location.uploadStore != "")
                return _duplicateErrorMessage("upload_store");
            if (!_parseSingleString(location.uploadStore, _tokens[_tokenIndex]))
                return (false);
        }
        else
        {
            std::cerr << "Error: Unknown directive " << _tokens[_tokenIndex] << " in location" << std::endl;
            return (false);
        }
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "}")
    {
        std::cerr << "Error: '}' expected at the end of location block" << std::endl;
        return (false);
    }
    ++_tokenIndex;
    return (true);
}

bool ConfigParser::_parseTokens()
{
    _tokenIndex = 0;
    _servers.clear();

    while (_tokenIndex < _tokens.size())
    {
        if (_tokens[_tokenIndex] != "server")
        {
            std::cerr << "Error: first token is not server" << std::endl;
            return (false);
        }
        ++_tokenIndex;
        ConfigServer server;
        if (!_parseServerBlock(server))
        {
            return false;
        }
        _servers.push_back(server); // push_back adds the new server object to _servers
    }
    return true;
}