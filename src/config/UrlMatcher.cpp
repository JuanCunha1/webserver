
#include "UrlMatcher.hpp"

const ConfigLocation* UrlMatcher::findBestLocation(const ConfigServer& server, const std::string& requestURI)
{
    const ConfigLocation* bestMatch = NULL;
    size_t longestMatchLength = 0;

    for (size_t i = 0; i < server.locations.size(); ++i)
    {
        const ConfigLocation& location = server.locations[i];
        const std::string& locationPath = location.path;

        if (requestURI.find(locationPath) == 0)
        {
            // bool for avoiding similar starting. ex: /api /api-v2
            bool isExactMatch = (requestURI == locationPath);
            bool isDirectoryBoundary = (locationPath[locationPath.length() - 1] == '/') || (requestURI.length() > locationPath.length() && requestURI[locationPath.length()] == '/');
            bool isRoot = (locationPath == "/");

            if (isExactMatch || isDirectoryBoundary || isRoot)
            {
                if (locationPath.length() > longestMatchLength)
                {
                    longestMatchLength = locationPath.length();
                    bestMatch = &server.locations[i];
                }
            }
        }
    }
    return (bestMatch);
}

const ConfigServer* UrlMatcher::findBestServer(const std::vector<ConfigServer>& servers, int clientPort, const std::string& clientHost)
{
    const ConfigServer* defaultServer = NULL;

    for (size_t i = 0; i < servers.size(); ++i)
    {
        if (servers[i].port == clientPort)
        {
            if (defaultServer == NULL)
            {
                defaultServer = &servers[i];
            }
            for (size_t j = 0; j < servers[i].serverNames.size(); ++j)
            {
                if (servers[i].serverNames[j] == clientHost)
                {
                    return &servers[i];
                }
            }
        }
    }
    return (defaultServer);
}