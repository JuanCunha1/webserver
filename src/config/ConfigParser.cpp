#include "config/ConfigParser.hpp"
#include <iostream>    // std::cout, std::cerr
#include <stdexcept>

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

    _openFile(file, filename);
    _readFileAndTokenize(file);
    file.close();
    
    _bracesChecker();
    _parseTokens();
    _validateSemantic();
    _postProcessConfiguration();
}