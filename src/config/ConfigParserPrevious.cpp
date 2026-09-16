#include "config/ConfigParser.hpp"
#include <iostream>    // std::cerr

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