#pragma once
#include "ConfigServer.hpp"

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
        bool    _openFile(std::ifstream& file, const std::string& filename);
        void    _readFileAndTokenize(std::ifstream& file);
        void    _notReadingCommentsInConfigFile(std::string& line);
        bool    _bracesChecker();

        // ConfigParserTokensBlocks.cpp
        bool    _parseTokens();
        bool    _parseServerBlock(ConfigServer& server);
        bool    _parseLocationBlock(ConfigLocation& location);
        bool    _duplicateErrorMessage(const std::string& directive);

        // ConfigParserTypes.cpp
        bool    _parseSingleString(std::string& target, const std::string& key);
        bool    _parseNumber(int& target, const std::string& key);
        bool    _parseVector(std::vector<std::string>& target, const std::string& key);
        bool    _parseReturn(std::vector<ConfigRedirections>& target);
        bool    _parseClientMaxBodySize(unsigned long& target, const std::string& valStr);
        
        // ConfigParserValidations.cpp
        bool    _validateSemantic();
        bool    _validatePathTraversal();
        bool    _validateRedirections();
        bool    _validateDirectoriesExist();
        void    _warnPrivilegedPorts();


        // ConfigParserPost.cpp
        bool    _postProcessConfiguration();
        void    _inheritServerToLocation();
        void    _normalizePaths();
        void    _normalizeCGIExtensions();
};