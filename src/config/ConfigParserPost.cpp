#include "ConfigParser.hpp"

// NGINX: CGI extensions always be like .etc (ex. php -> .php)
void ConfigParser::_normalizeCGIExtensions()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];
            for (size_t k = 0; k < loc.cgiExtension.size(); ++k)
            {
                std::string& ext = loc.cgiExtension[k];
                if (!ext.empty() && ext[0] == '*')
                    ext.erase(0, 1); // erase(initial position, nº characters)
                if (!ext.empty() && ext[0] != '.')
                    ext = "." + ext;
            }
        }
    }
}

// NGINX: We take / from /var/www, etc for avoiding /var/www//index.html conflicts
void ConfigParser::_normalizePaths()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        if (_servers[i].root.length() > 1 && _servers[i].root[_servers[i].root.length() - 1] == '/')
            _servers[i].root.erase(_servers[i].root.length() - 1);
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];
            if (loc.locationRoot.length() > 1 && loc.locationRoot[loc.locationRoot.length() - 1] == '/')
                loc.locationRoot.erase(loc.locationRoot.length() - 1);
            if (loc.path.length() > 1 && loc.path[loc.path.length() - 1] == '/')
                loc.path.erase(loc.path.length() - 1);
            if (loc.uploadStore.length() > 1 && loc.uploadStore[loc.uploadStore.length() - 1] == '/')
                loc.uploadStore.erase(loc.uploadStore.length() - 1);
        }
    }
}

// NGINX: location inherits root and index from its server block
void ConfigParser::_inheritServerToLocation()
{
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        for (size_t j = 0; j < _servers[i].locations.size(); ++j)
        {
            ConfigLocation& loc = _servers[i].locations[j];
            if (loc.locationRoot == "")
                loc.locationRoot = _servers[i].root;
            if (loc.indexFile == "")
                loc.indexFile = _servers[i].indexFile;
            if (!loc.isAutoindexDefined && _servers[i].isAutoindexDefined)
            {
                loc.autoindex = _servers[i].autoindex;
                loc.isAutoindexDefined = true;
            }
        }
    }
}

bool ConfigParser::_postProcessConfiguration()
{
    _inheritServerToLocation();
    _normalizePaths();
    _normalizeCGIExtensions();
    return (true);
}