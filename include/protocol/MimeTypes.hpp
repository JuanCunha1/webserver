#pragma once

#include <string>
#include <map>

class MimeTypes {
public:
	static std::string getType(const std::string& path);
	static std::string getExtension(const std::string& path);

private:
	MimeTypes();
	static std::map<std::string, std::string> _types;
	static void init();
};