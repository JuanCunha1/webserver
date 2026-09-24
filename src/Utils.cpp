#include "../include/Utils.hpp"
#include <ctime>
#include <string>

bool Utils::isAllUpper(const std::string& str) {
	if (str.empty()) {
		return false;
	}
	for (std::size_t i = 0; i < str.length(); ++i) {
		if (!std::isupper(static_cast<unsigned char>(str[i]))) {
			return false;
		}
	}
	return true;
}

std::string Utils::getCurrentDateGMT() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[100];

    time(&rawtime);
    timeinfo = gmtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);

    return std::string(buffer);
}