#include "ConfigParser.hpp"
#include <iostream>
#include <cstdlib> // std::atoi
#include <cctype> // std::isdigit

ConfigParser::ConfigParser()
{

}

ConfigParser::ConfigParser( const ConfigParser& original )
{
    *this = original;
}

ConfigParser& ConfigParser::operator=( const ConfigParser& rhs ) {
    (void)rhs; // Not var to copy yet
    return *this;
}

ConfigParser::~ConfigParser()
{

}

const std::vector<ConfigServer>& ConfigParser::getServers() const
{
    return _servers;
}

bool ConfigParser::_validateSemantic()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
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
    for (size_t i = 0; i < _servers.size(); ++i)
        {
            for (size_t j = i + 1; j < _servers.size(); ++j)
            {
                if (_servers[i].port == _servers[j].port && _servers[i].host == _servers[j].host)
                {
                    std::cerr << "Error: Multiple servers cannot listen on the same host " << _servers[i].host << " and port " << _servers[i].port << std::endl;
                    return (false);
                }
            }
        }
    return (true);
}

bool ConfigParser::_parseReturn(std::vector<ConfigRedirections>& target)
{
    ++_tokenIndex;
    std::vector<std::string> args;
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != ";")
    {
        args.push_back(_tokens[_tokenIndex]);
        ++_tokenIndex;
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";" || args.empty() || args.size() > 2)
    {
        std::cerr << "Error: Invalid return directive format." << std::endl;
        return (false);
    }
    ConfigRedirections redir;
    if (args.size() == 2)
    {
        redir.returnCode = std::atoi(args[0].c_str());
        redir.returnUrl = args[1];
    }
    else if (args.size() == 1)
    {
        redir.returnCode = 302; // Temporal redirection. NGINX default number
        redir.returnUrl = args[0];
    }
    target.push_back(redir);
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
            if (!_parseSingleString(location.locationRoot, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
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

bool ConfigParser::_parseVector(std::vector<std::string>& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        std::cerr << "Error: '" << key << "' must have at least one value" << std::endl;
        return (false);
    }
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != ";")
    {
        target.push_back(_tokens[_tokenIndex]);
        ++_tokenIndex;
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        std::cerr << "Error: ';' missing after '" << key << "' list" << std::endl;
        return (false);
    }
    ++_tokenIndex;
    return (true);
}

bool ConfigParser::_parseNumber(int& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        std::cerr << "Error: '" << key << "' must be a number" << std::endl;
        return (false);
    }
    const std::string& valStr = _tokens[_tokenIndex];
    for (size_t i = 0; i < valStr.length(); ++i)
    {
        if (!std::isdigit(valStr[i]))
        {
            std::cerr << "Error: Invalid number " << valStr << " in " << key << std::endl;
            return (false);
        }
    }
    target = std::atoi(valStr.c_str());
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        std::cerr << "Error: ';' missing after " << key << std::endl;
        return (false);
    }
    ++_tokenIndex;
    return (true);
}

bool ConfigParser::_parseSingleString(std::string& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        std::cerr << "Error: '" << key << "' must have a value" << std::endl;
        return (false);
    }
    target = _tokens[_tokenIndex];
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        std::cerr << "Error: ';' missing after " << key << std::endl;
        return (false);
    }
    ++_tokenIndex;
    return (true);
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
            if (!_parseNumber(server.port, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "host")
        {
            if (!_parseSingleString(server.host, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "root")
        {
            if (!_parseSingleString(server.root, _tokens[_tokenIndex]))
                return (false);
        }
        else if (_tokens[_tokenIndex] == "index")
        {
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
            ++_tokenIndex;
            if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
            {
                std::cerr << "Error: 'client_max_body_size' must have a value" << std::endl;
                return (false);
            }
            const std::string& verificationOnlyNumber = _tokens[_tokenIndex];
            for (size_t i = 0; i < verificationOnlyNumber.length(); ++i)
            {
                if (!std::isdigit(verificationOnlyNumber[i]))
                {
                    std::cerr << "Error: Invalid number " << verificationOnlyNumber << " in client_max_body_size" << std::endl;
                    return (false);
                }
            }
            server.clientMaxBodySize = std::strtoul(_tokens[_tokenIndex].c_str(), NULL, 10);
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

bool ConfigParser::_bracesChecker()
{
    int  balanced = 0;

    for (size_t i = 0; i < _tokens.size(); ++i) // i < var.size because vector has no NULL terminator 
    {
        if (_tokens[i] == "{")
        {
            balanced++;
        }
        else if (_tokens[i] == "}")
        {
            balanced--;
        }
        if (balanced < 0)
        {
            return (false);
        }
    }
    if (balanced == 0)
    {
        return (true);
    }
    return (false);
}

void ConfigParser::_notReadingCommentsInConfigFile(std::string& line)
{
    size_t pos = line.find('#');
    
    if (pos != std::string::npos)
    {
        line = line.substr(0, pos);
    }
}

void ConfigParser::_readFileAndTokenize(std::ifstream& file)
{
    std::string line;
    
    while (std::getline(file, line))
    {
        _notReadingCommentsInConfigFile(line);
        size_t start = 0;
        std::string auxSpacedLine = "";

        for (size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == ';' || line[i] == '{' || line[i] == '}')
            {
                auxSpacedLine += " ";
                auxSpacedLine += line[i];
                auxSpacedLine += " ";
            }
            else
            {
                auxSpacedLine += line[i];
            }
        }
        line = auxSpacedLine;
        while (true)
        {
            start = line.find_first_not_of(" \t\r\n", start);
            
            if (start == std::string::npos)
            {
                break;
            }

            size_t end = line.find_first_of(" \t\r\n", start);
            if (end == std::string::npos)
            {
                _tokens.push_back(line.substr(start));
                break;
            }
            else
            {
                _tokens.push_back(line.substr(start, end - start));
                start = end;
            }
        }
    }
}

bool ConfigParser::_openFile(std::ifstream& file, const std::string& filename)
{
    file.open(filename.c_str()); // c_str for "translating" to C
    
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return false;
    }
    return true;
}

void ConfigParser::parseFile(const std::string& filename)
{
    std::ifstream file;

    if (!_openFile(file, filename))
    {
        return; 
    }
    _readFileAndTokenize(file);
    file.close();
    if (!_bracesChecker())
    {
        std::cerr << "Error: Incorrect braces in " << filename << std::endl;
        return;
    }
    if (!_parseTokens())
    {
        std::cerr << "Error: Configuration parser failed" << std::endl;
        return;
    }
    if (!_validateSemantic())
    {
        _servers.clear(); // Clean memory if needed
        return;
    }
}

/*
-----------------SIMPLE MAIN FOR TESTING

#include "ConfigParser.hpp"

int main()
{
    ConfigParser parser;
    parser.parseFile("test.conf");
    return (0);
}
*/