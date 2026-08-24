#pragma once

class Request {
	private:
		std::string method;
		std::string uri;
		std::string query;
		std::string version;
		std::map<std::string, std::string> headers;
		std::string body;

		bool isComplete;
		int errorCode;

	public:
		Request();
		Request(const Request &src);
		Request &operator=(const Request &rhs);
		~Request();

		std::string &getMethod() const { return (this->method); }
		std::string &getUri() const { return (this->uri); }
		std::string &getQuery() const { return (this->query); }
		std::string &getVersion() const { return (this->version); }
		std::string &getHeader(const std::string &key) const {
			return (this->find(key)->second);
		}
		std::string &getBody() const { (return this->body); }


};