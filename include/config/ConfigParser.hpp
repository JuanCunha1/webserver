#pragma once
#include "ConfigServer.hpp"

#include <vector>
#include <string>
#include <fstream> // File Stream: to read and write files on disk

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

        // ConfigParserPreviousParseToken.cpp
        bool    _openFile(std::ifstream& file, const std::string& filename);
        void    _readFileAndTokenize(std::ifstream& file);
        void    _notReadingCommentsInConfigFile(std::string& line);
        bool    _bracesChecker();

        // ConfigParserTokensBlocks.cpp
        bool    _parseTokens();
        bool    _parseServerBlock(ConfigServer& server);
        bool    _parseLocationBlock(ConfigLocation& location);

        //ConfigParserTypes.cpp
        bool    _parseSingleString(std::string& target, const std::string& key);
        bool    _parseNumber(int& target, const std::string& key);
        bool    _parseVector(std::vector<std::string>& target, const std::string& key);
        bool    _parseReturn(std::vector<ConfigRedirections>& target);

        bool    _validateSemantic();
};