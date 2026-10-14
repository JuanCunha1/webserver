#include "protocol/CgiHandler.hpp"
#include "protocol/Response.hpp"
#include "protocol/Request.hpp"
#include "protocol/HttpException.hpp"

#include "Utils.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <cerrno>
#include <stdlib.h>
#include <limits.h>

CgiHandler::CgiHandler() 
	: _pid(-1), _pipeIn(-1), _pipeOut(-1), 
	  _isWriteDone(false), _isReadDone(false), _hasError(false) {}

CgiHandler::~CgiHandler() {
	cleanup();
}

void CgiHandler::cleanup() {
	if (_pipeIn != -1) {
		close(_pipeIn);
		_pipeIn = -1;
	}
	if (_pipeOut != -1) {
		close(_pipeOut);
		_pipeOut = -1;
	}
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0); 
		_pid = -1;
	}
}

void CgiHandler::killCgi() {
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		_pid = -1;
	}
	
	// Close pipes so the event loop does not try to read any more
	if (_pipeIn != -1) {
		close(_pipeIn); _pipeIn = -1;
	}
	if (_pipeOut != -1) {
		close(_pipeOut); _pipeOut = -1;
	}
	
	// Set the state so buildCgiResponse knows what to do
	_isReadDone = true;
	_isWriteDone = true;
	_hasError = true;
}

int CgiHandler::getReadFd() const {
	return (_pipeOut);
}

int CgiHandler::getWriteFd() const {
	return (_pipeIn);
}

pid_t CgiHandler::getPid() const {
	return (_pid);
}

bool CgiHandler::isReadDone() const {
	return (_isReadDone);
}

bool CgiHandler::isWriteDone() const {
	return (_isWriteDone);
}

bool CgiHandler::hasError() const {
	return (_hasError);
}

bool CgiHandler::createPipes(int pIn[2], int pOut[2]) {
	if (pipe(pIn) == -1) {
		_hasError = true;
		return (false);
	}
	if (pipe(pOut) == -1) {
		close(pIn[0]);
		close(pIn[1]);
		_hasError = true;
		return (false);
	}

	fcntl(pIn[1], F_SETFL, O_NONBLOCK);
	fcntl(pOut[0], F_SETFL, O_NONBLOCK);

	return (true);
}

void CgiHandler::executeChild(int pIn[2], int pOut[2], const Request &req, 
							  const std::string &scriptPath, const std::string &cgiBinary) {
	if (dup2(pIn[0], STDIN_FILENO) == -1 || dup2(pOut[1], STDOUT_FILENO) == -1) {
		std::exit(EXIT_FAILURE);
	}

	close(pIn[0]);
	close(pIn[1]);
	close(pOut[0]);
	close(pOut[1]);

	//* Split path and file name
	std::string scriptDir = "."; 
	std::string scriptName = scriptPath;
	size_t lastSlash = scriptPath.find_last_of('/');
	
	if (lastSlash != std::string::npos) {
		scriptDir = scriptPath.substr(0, lastSlash);
		scriptName = scriptPath.substr(lastSlash + 1);
	}

	//* Move to the CGI directory
	if (chdir(scriptDir.c_str()) == -1) {
		_exit(EXIT_FAILURE);
	}

	char **argv = buildArgv(scriptName, cgiBinary);
	char **envp = buildEnv(req, scriptName);

	execve(argv[0], argv, envp);
	
	freeCharArray(argv);
	freeCharArray(envp);
	_exit(127);
}

void CgiHandler::setupParent(int pIn[2], int pOut[2], const Request &req) {
	close(pIn[0]);
	close(pOut[1]);

	_pipeIn = pIn[1];
	_pipeOut = pOut[0];

	if (req.getMethod() == "POST") {
		_inputBuffer = req.getBody();
		if (_inputBuffer.empty()) {
			close(_pipeIn);
			_pipeIn = -1;
			_isWriteDone = true;
		}
	} else {
		close(_pipeIn);
		_pipeIn = -1;
		_isWriteDone = true;
	}
}

bool CgiHandler::initCgi(const Request &req, const std::string &scriptPath, const std::string &cgiBinary) {
	int pIn[2];
	int pOut[2];

	if (!createPipes(pIn, pOut)) {
		return (false);
	}

	_pid = fork();
	if (_pid == -1) {
		close(pIn[0]); close(pIn[1]);
		close(pOut[0]); close(pOut[1]);
		_hasError = true;
		return (false);
	}

	if (_pid == 0) {
		executeChild(pIn, pOut, req, scriptPath, cgiBinary);
	} else {
		setupParent(pIn, pOut, req);
	}
	return (true);
}

void CgiHandler::writeToCgi() {
	if (_pipeIn == -1 || _inputBuffer.empty()) {
		return;
	}

	ssize_t written = write(_pipeIn, _inputBuffer.c_str(), _inputBuffer.size());
	if (written <= 0) {
		close(_pipeIn);
		_pipeIn = -1;
		_isWriteDone = true;
		_hasError = true;
		return;
	}

	_inputBuffer.erase(0, written);
	if (_inputBuffer.empty()) {
		close(_pipeIn);
		_pipeIn = -1;
		_isWriteDone = true;
	}
}

void CgiHandler::readFromCgi() {
    if (_pipeOut == -1) {
        return;
    }
    char buffer[4096];
    ssize_t bytesRead = read(_pipeOut, buffer, sizeof(buffer));

    if (bytesRead > 0) {
        _outputBuffer.append(buffer, bytesRead);
        return;
    }

    close(_pipeOut);
    _pipeOut = -1;
    _isReadDone = true;

    if (bytesRead < 0) {
        _hasError = true;
        return;
    }

    if (_pid > 0) {
        int status = 0;
        pid_t wp = waitpid(_pid, &status, WNOHANG);

        if (wp > 0) {
            _pid = -1;
            if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) {
                _hasError = true;
            }
        } else if (wp == -1 && errno == ECHILD) {
            _pid = -1;
        }
    }
}

bool CgiHandler::splitOutput(std::string &headersPart, std::string &bodyPart) {
	size_t headerEnd = _outputBuffer.find("\r\n\r\n");
	size_t delimiterLen = 4;

	if (headerEnd == std::string::npos) {
		headerEnd = _outputBuffer.find("\n\n");
		delimiterLen = 2;
	}

	if (headerEnd == std::string::npos) {
		return (false);
	}

	headersPart = _outputBuffer.substr(0, headerEnd);
	bodyPart = _outputBuffer.substr(headerEnd + delimiterLen);
	return (true);
}

void CgiHandler::parseHeaders(const std::string &headersPart, Response &res, 
							  int &statusCode, std::string &statusMessage) {
	std::istringstream stream(headersPart);
	std::string line;

	while (std::getline(stream, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r') {
			line.erase(line.size() - 1);
		}
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos) {
			continue;
		}

		std::string key = line.substr(0, colonPos);
		std::string val = line.substr(colonPos + 1);

		while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) {
			val.erase(0, 1);
		}

		if (key == "Status") {
			std::istringstream statusStream(val);
			statusStream >> statusCode;
			std::getline(statusStream, statusMessage);
			while (!statusMessage.empty() && statusMessage[0] == ' ') {
				statusMessage.erase(0, 1);
			}
		} else {
			res.setHeader(key, val);
		}
	}
}

Response CgiHandler::buildCgiResponse() {
	Response res;

	if (_hasError) {
		std::string errorBody = "<html><body><h1>500 Internal Server Error</h1><p>CGI Execution Failed.</p></body></html>";
		res.setStatusCode(500);
		res.setStatusMessage("Internal Server Error");
		res.setHeader("Content-Type", "text/html");
		res.setHeader("Content-Length", Utils::toString(errorBody.size()));
		res.setBody(errorBody);
		return (res);
	}

	std::string headersPart;
	std::string bodyPart;

	if (!splitOutput(headersPart, bodyPart)) {
		res.setStatusCode(502);
		res.setStatusMessage("Bad Gateway");
		return (res);
	}

	int statusCode = 200;
	std::string statusMessage = "OK";

	parseHeaders(headersPart, res, statusCode, statusMessage);

	res.setStatusCode(statusCode);
	res.setStatusMessage(statusMessage);
	res.setBody(bodyPart);
	res.setHeader("Content-Length", Utils::toString(bodyPart.size()));

	return (res);
}

char** CgiHandler::buildArgv(const std::string &scriptName, const std::string &cgiBinary) {
	char **argv = new char*[3];

	if (cgiBinary.empty() || cgiBinary == "\"\"" || cgiBinary == "''") {
		// For compiled CGIs (e.g. ubuntu_cgi_tester)
		std::string execPath = "./" + scriptName;
		argv[0] = new char[execPath.size() + 1];
		std::strcpy(argv[0], execPath.c_str());

		argv[1] = new char[scriptName.size() + 1];
		std::strcpy(argv[1], scriptName.c_str());
	} else {
		// For interpreted scripts (Python/PHP)
		argv[0] = new char[cgiBinary.size() + 1];
		std::strcpy(argv[0], cgiBinary.c_str());

		argv[1] = new char[scriptName.size() + 1];
		std::strcpy(argv[1], scriptName.c_str());
	}

	argv[2] = NULL;
	return (argv);
}

char** CgiHandler::buildEnv(const Request &req, const std::string &scriptPath) {
	std::vector<std::string> envVector;

	envVector.push_back("GATEWAY_INTERFACE=CGI/1.1");
	envVector.push_back("SERVER_PROTOCOL=HTTP/1.1");
	envVector.push_back("REQUEST_METHOD=" + req.getMethod());
	envVector.push_back("SCRIPT_FILENAME=" + scriptPath);
	envVector.push_back("SCRIPT_NAME=" + req.getUri());
	envVector.push_back("REDIRECT_STATUS=200");

	if (req.getMethod() == "GET") {
		envVector.push_back("QUERY_STRING=" + req.getQuery());
	} else if (req.getMethod() == "POST") {
		envVector.push_back("QUERY_STRING=" + req.getQuery());
		const std::string *cType = req.getHeader("content-type");
		if (cType) {
			envVector.push_back("CONTENT_TYPE=" + *cType);
		}
		envVector.push_back("CONTENT_LENGTH=" + Utils::toString(req.getBody().size()));
	}

	char **envp = new char*[envVector.size() + 1];
	for (size_t i = 0; i < envVector.size(); ++i) {
		envp[i] = new char[envVector[i].size() + 1];
		std::strcpy(envp[i], envVector[i].c_str());
	}
	envp[envVector.size()] = NULL;

	return (envp);
}

void CgiHandler::freeCharArray(char **arr) {
	if (!arr) {
		return;
	}
	for (size_t i = 0; arr[i] != NULL; ++i) {
		delete[] arr[i];
	}
	delete[] arr;
}