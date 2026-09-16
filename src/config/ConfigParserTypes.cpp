#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr
#include <cstdlib>     // std::atoi, std::strtoul
#include <cctype>      // std::isdigit

// NGINX: letters to bytes 
bool ConfigParser::_parseClientMaxBodySize(unsigned long& target, const std::string& valStr)
{
    if (valStr.empty())
        return (false);

    char lastChar = valStr[valStr.length() - 1];
    std::string numberPart = valStr;
    unsigned long multiplier = 1;

    if (!std::isdigit(lastChar))
    {
        if (lastChar == 'K' || lastChar == 'k')
            multiplier = 1024;
        else if (lastChar == 'M' || lastChar == 'm')
            multiplier = 1024 * 1024;
        else if (lastChar == 'G' || lastChar == 'g')
            multiplier = 1024 * 1024 * 1024;
        else
        {
            std::cerr << "Error: Invalid unit " << lastChar << " in body size" << std::endl;
            return (false);
        }
        numberPart = valStr.substr(0, valStr.length() - 1);
    }
    for (size_t i = 0; i < numberPart.length(); ++i)
    {
        if (!std::isdigit(numberPart[i]))
        {
            std::cerr << "Error: Invalid number format in body size." << std::endl;
            return (false);
        }
    }
    target = std::strtoul(numberPart.c_str(), NULL, 10) * multiplier;
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