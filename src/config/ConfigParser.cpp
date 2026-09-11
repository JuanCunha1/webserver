#include "ConfigParser.hpp"
#include <iostream>    // std::cout, std::cerr

ConfigParser::ConfigParser()
{

}

ConfigParser::ConfigParser( const ConfigParser& original )
{
    *this = original;
}

ConfigParser& ConfigParser::operator=( const ConfigParser& rhs ) {
    if (this != &rhs)
    {
        this->_tokens = rhs._tokens;
        this->_tokenIndex = rhs._tokenIndex;
        this->_servers = rhs._servers;
    }
    return *this;
}

ConfigParser::~ConfigParser()
{

}

const std::vector<ConfigServer>& ConfigParser::getServers() const
{
    return _servers;
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
    if (!_postProcessConfiguration())
    {
        _servers.clear();
        return;
    }
}