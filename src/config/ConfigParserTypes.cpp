#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr
#include <cstdlib>     // std::atoi, std::strtoul
#include <cctype>      // std::isdigit
#include <stdexcept>

// NGINX: letters to bytes 
void ConfigParser::_parseClientMaxBodySize(unsigned long& target, const std::string& valStr)
{
    if (valStr.empty())
    {
        throw std::runtime_error("Error: Empty body size value");
    }

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
            throw std::runtime_error("Configuration error");
        }
        numberPart = valStr.substr(0, valStr.length() - 1);
    }
    for (size_t i = 0; i < numberPart.length(); ++i)
    {
        if (!std::isdigit(numberPart[i]))
        {
            throw std::runtime_error("Error: Invalid number format in body size.");
        }
    }
    target = std::strtoul(numberPart.c_str(), NULL, 10) * multiplier;
}

void ConfigParser::_parseReturn(std::vector<ConfigRedirections>& target)
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
        throw std::runtime_error("Error: Invalid return directive format.");
    }
    
    ConfigRedirections redir;
    if (args.size() == 2)
    {
        redir.returnCode = std::atoi(args[0].c_str());
        redir.returnUrl = args[1];
    }
    else if (args.size() == 1)
    {
        redir.returnCode = 302;
        redir.returnUrl = args[0];
    }
    target.push_back(redir);
    ++_tokenIndex;
}

void ConfigParser::_parseVector(std::vector<std::string>& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        throw std::runtime_error("Error: '" + key + "' must have at least one value");
    }
    
    while (_tokenIndex < _tokens.size() && _tokens[_tokenIndex] != ";")
    {
        target.push_back(_tokens[_tokenIndex]);
        ++_tokenIndex;
    }
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        throw std::runtime_error("Error: ';' missing after '" + key + "' list");
    }
    ++_tokenIndex;
}

void ConfigParser::_parseNumber(int& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        throw std::runtime_error("Error: '" + key + "' must be a number");
    }
    
    const std::string& valStr = _tokens[_tokenIndex];
    for (size_t i = 0; i < valStr.length(); ++i)
    {
        if (!std::isdigit(valStr[i]))
        {
            throw std::runtime_error("Error: Invalid number " + valStr + " in " + key);
        }
    }
    target = std::atoi(valStr.c_str());
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        throw std::runtime_error("Error: ';' missing after " + key);
    }
    ++_tokenIndex;
}

void ConfigParser::_parseSingleString(std::string& target, const std::string& key)
{
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] == ";")
    {
        throw std::runtime_error("Error: '" + key + "' must have a value");
    }
    
    target = _tokens[_tokenIndex];
    ++_tokenIndex;
    if (_tokenIndex >= _tokens.size() || _tokens[_tokenIndex] != ";")
    {
        throw std::runtime_error("Error: ';' missing after " + key);
    }
    ++_tokenIndex;
}