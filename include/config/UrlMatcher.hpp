#pragma once

#include "ConfigServer.hpp"
#include <vector>
#include <string>

class UrlMatcher
{
    public:
        // Filters by client port and matches the 'Host' header against server_names. (Virtual Host)
        static const ConfigServer* findBestServer(const std::vector<ConfigServer>& servers, int clientPort, const std::string& clientHost);

        // Once the server is found, matches the exact path using Longest Prefix Match.
        static const ConfigLocation* findBestLocation(const ConfigServer& server, const std::string& requestURI);
};