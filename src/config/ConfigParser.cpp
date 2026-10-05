#include "config/ConfigParser.hpp"
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
        throw std::runtime_error("Error: Cannot open file " + filename);
    }
    
    _readFileAndTokenize(file);
    file.close();
    
    if (!_bracesChecker())
    {
        throw std::runtime_error("Error: Incorrect braces in " + filename);
    }
    
    if (!_parseTokens())
    {
        throw std::runtime_error("Error: Configuration parser failed");
    }
    
    if (!_validateSemantic())
    {
        _servers.clear(); // Limpiamos memoria si es necesario
        throw std::runtime_error("Error: Semantic validation failed");
    }
    
    if (!_postProcessConfiguration())
    {
        _servers.clear();
        throw std::runtime_error("Error: Post-processing configuration failed");
    }
}
