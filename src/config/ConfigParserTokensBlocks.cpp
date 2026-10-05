#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr
#include <stdexcept>
#include <cstdlib>

void ConfigParser::_duplicateErrorMessage(const std::string& directive)
{
    throw std::runtime_error("Error: '" + directive + "' directive is duplicated.");
}

void ConfigParser::_parseServerBlock(ConfigServer& server)
{
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "{")
    {
        throw std::runtime_error("Error: '{' expected after server");
    }
    ++_tokenIndex;
    
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != "}")
    {
        if (_tokens[_tokenIndex] == "listen")
        {
            if (server.port != -1)
                _duplicateErrorMessage("listen");
            _parseNumber(server.port, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "host")
        {
            if (server.host != "")
                _duplicateErrorMessage("host");
            _parseSingleString(server.host, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "root")
        {
            if (server.root != "")
                _duplicateErrorMessage("root");
            _parseSingleString(server.root, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
            if (server.indexFile != "")
                _duplicateErrorMessage("index");
            _parseSingleString(server.indexFile, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "server_name")
        {
            _parseVector(server.serverNames, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "client_max_body_size")
        {
            if (server.clientMaxBodySize != 0)
                _duplicateErrorMessage("client_max_body_size");
            ++_tokenIndex;
            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
                throw std::runtime_error("Error: 'client_max_body_size' must have a value");
            _parseClientMaxBodySize(server.clientMaxBodySize, _tokens[_tokenIndex]);
            ++_tokenIndex;
            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
                throw std::runtime_error("Error: ';' missing after 'client_max_body_size'");
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
                throw std::runtime_error("Error: Error page not written correctly");
            }
            
            ErrorPages ePages;
            ePages.errorPath = args.back();
            for (size_t i = 0; i < args.size() - 1; ++i)
            {
                for (size_t j = 0; j < args[i].length(); ++j)
                {
                    if (!std::isdigit(args[i][j]))
                    {
                        throw std::runtime_error("Error: Invalid error code in error_page");
                    }
                }
                ePages.errorCodes.push_back(std::atoi(args[i].c_str()));
            }
            server.errorPages.push_back(ePages);
            ++_tokenIndex;
        }
        else if (_tokens[_tokenIndex] == "location")
        {
            ConfigLocation loc;
            _parseLocationBlock(loc);
            server.locations.push_back(loc);
        }
        else if (_tokens[_tokenIndex] == "autoindex")
        {
            if (server.isAutoindexDefined)
                _duplicateErrorMessage("autoindex");
            std::string autoindexStr;
            _parseSingleString(autoindexStr, _tokens[_tokenIndex]);
            server.isAutoindexDefined = true;
            if (autoindexStr == "on")
                server.autoindex = true;
            else if (autoindexStr == "off")
                server.autoindex = false;
            else
                throw std::runtime_error("Error: Invalid autoindex value (must be 'on' or 'off')");
        }
        else
        {
            throw std::runtime_error("Error: Unknown key " + _tokens[_tokenIndex] + " in server");
        }
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "}")
    {
        throw std::runtime_error("Error: '}' expected at the end of the server block");
    }
    ++_tokenIndex;
}

void ConfigParser::_parseLocationBlock(ConfigLocation& location)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == "{" || _tokens[_tokenIndex] == ";")
    {
        throw std::runtime_error("Error: Location path missing");
    }
    location.path = _tokens[_tokenIndex];
    
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "{")
    {
        throw std::runtime_error("Error: '{' expected after location path");
    }
    ++_tokenIndex;
    
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != "}")
    {
        if (_tokens[_tokenIndex] == "root")
        {
            if (location.locationRoot != "")
                _duplicateErrorMessage("root");
            _parseSingleString(location.locationRoot, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
            if (location.indexFile != "")
                _duplicateErrorMessage("index");
            _parseSingleString(location.indexFile, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "allow_methods")
        {
            _parseVector(location.allowedMethods, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "autoindex")
        {
            std::string autoindexStr;
            _parseSingleString(autoindexStr, _tokens[_tokenIndex]);
            location.isAutoindexDefined = true;
            if (autoindexStr == "on")
                location.autoindex = true;
            else if (autoindexStr == "off")
                location.autoindex = false;
            else
                throw std::runtime_error("Error: Invalid autoindex value (must be 'on' or 'off')");
        }
        else if (_tokens[_tokenIndex] == "return")
        {
            _parseReturn(location.returnRedirections);
        }
        else if (_tokens[_tokenIndex] == "cgi_path")
        {
            _parseVector(location.cgiPath, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "cgi_extension")
        {
            _parseVector(location.cgiExtension, _tokens[_tokenIndex]);
        }
        else if (_tokens[_tokenIndex] == "upload_store")
        {
            if (location.uploadStore != "")
                _duplicateErrorMessage("upload_store");
            _parseSingleString(location.uploadStore, _tokens[_tokenIndex]);
        }
        else
        {
            throw std::runtime_error("Error: Unknown directive " + _tokens[_tokenIndex] + " in location");
        }
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != "}")
    {
        throw std::runtime_error("Error: '}' expected at the end of location block");
    }
    ++_tokenIndex;
}

void ConfigParser::_parseTokens()
{
    _tokenIndex = 0;
    _servers.clear();

    while (_tokenIndex < _tokens.size())
    {
        if (_tokens[_tokenIndex] != "server")
        {
            throw std::runtime_error("Error: first token is not server");
        }
        ++_tokenIndex;
        ConfigServer server;
        _parseServerBlock(server);
        _servers.push_back(server); // push_back adds the new server object to _servers
    }
}