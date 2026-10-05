#pragma once
#include "config/ConfigServer.hpp"

#include <vector>
#include <string>
#include <fstream> // File Stream: to read and write files on disk

#include <iostream>
#include <cstdlib> // std::atoi
#include <cctype> // std::isdigit
#include <sys/stat.h>

class ConfigParser
{
    public:
        // ConfigParser.cpp
        ConfigParser();
        ConfigParser( const ConfigParser& original );
        ConfigParser& operator=( const ConfigParser& rhs );
        ~ConfigParser();
        const std::vector<ConfigServer>& getServers() const;
        void parseFile(const std::string& file);

    private:
        std::vector<std::string>    _tokens;
        size_t                      _tokenIndex;
        std::vector<ConfigServer>   _servers;

        // ConfigParserPrevious.cpp
        void    _openFile(std::ifstream& file, const std::string& filename);
        void    _readFileAndTokenize(std::ifstream& file);
        void    _notReadingCommentsInConfigFile(std::string& line);
        void    _bracesChecker();

        // ConfigParserTokensBlocks.cpp
        void    _parseTokens();
        void    _parseServerBlock(ConfigServer& server);
        void    _parseLocationBlock(ConfigLocation& location);
        void    _duplicateErrorMessage(const std::string& directive);

        // ConfigParserTypes.cpp
        void    _parseSingleString(std::string& target, const std::string& key);
        void    _parseNumber(int& target, const std::string& key);
        void    _parseVector(std::vector<std::string>& target, const std::string& key);
        void    _parseReturn(std::vector<ConfigRedirections>& target);
        void    _parseClientMaxBodySize(unsigned long& target, const std::string& valStr);
        
        // ConfigParserValidations.cpp
        void    _validateSemantic();
        void    _validatePathTraversal();
        void    _validateRedirections();
        void    _validateDirectoriesExist();
        void    _warnPrivilegedPorts();


        // ConfigParserPost.cpp
        bool    _postProcessConfiguration();
        void    _inheritServerToLocation();
        void    _normalizePaths();
        void    _normalizeCGIExtensions();
};