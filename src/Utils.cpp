#include "../include/Utils.hpp"
#include <ctime>
#include <string>
#include <sstream>

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

//* Transforma los caracteres tipo %2F(2F hexadecimal de la tabla ascii) en su caracter
std::string Utils::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            // Asegurarnos de que quedan al menos 2 caracteres después del '%'
            if (i + 2 < str.length()) {
                int value;
                std::istringstream is(str.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += '%'; // Si falla la conversión, lo dejamos igual
                }
            } else {
                result += '%';
            }
        } else if (str[i] == '+') {
            // '+' en una URL == espacio
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}