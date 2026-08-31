#include "../../include/protocol/MimeTypes.hpp"

std::map<std::string, std::string> MimeTypes::_types;

void MimeTypes::init() {
	if (_types.empty()) {
		_types[".html"] = "text/html";
		_types[".css"]  = "text/css";
		_types[".js"]   = "application/javascript";
		_types[".png"]  = "image/png";
		_types[".jpg"]  = "image/jpeg";
		_types[".txt"]  = "text/plain";
	}
}

std::string MimeTypes::getExtension(const std::string &path) {
	std::string::size_type dot_pos = path.rfind('.');
	if (dot_pos == std::string::npos || dot_pos == path.length() - 1)
		return ("");
	if (path.find_first_of("/\\", dot_pos) != std::string::npos)
		return ("");
	return (path.substr(dot_pos));
}

std::string MimeTypes::getType(const std::string &path) {
	init();
	std::string ext = getExtension(path);
	std::map<std::string, std::string>::const_iterator it = _types.find(ext);
	if (it != _types.end())
		return (it->second);
	return ("application/octet-stream");
}