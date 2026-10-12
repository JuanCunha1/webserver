#pragma once
#include <string>
#include <vector>
#include <map>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

//* Cgi must be an independent object 

class Request;
class Response;

class CgiHandler {
	private:
		pid_t		_pid;
		int			_pipeIn;        //* FD to the CGI's STDIN (write)
		int			_pipeOut;       //* FD from the CGI's STDOUT (read)
		
		std::string	_inputBuffer;
		std::string	_outputBuffer;
		
		bool		_isWriteDone;
		bool		_isReadDone;
		bool		_hasError;

		// Private helper methods for initCgi
		bool		createPipes(int pIn[2], int pOut[2]);
		void		executeChild(int pIn[2], int pOut[2], const Request &req, 
								const std::string &scriptPath, const std::string &cgiBinary);
		void		setupParent(int pIn[2], int pOut[2], const Request &req);

		// Private helper methods for buildCgiResponse
		bool		splitOutput(std::string &headersPart, std::string &bodyPart);
		void		parseHeaders(const std::string &headersPart, Response &res, 
								 int &statusCode, std::string &statusMessage);

		char**		buildArgv(const std::string &scriptPath, const std::string &cgiBinary);
		char**		buildEnv(const Request &req, const std::string &scriptPath);
		void		freeCharArray(char **arr);

	public:
		CgiHandler();
		~CgiHandler();

		bool		initCgi(const Request &req, const std::string &scriptPath, const std::string &cgiBinary);

		void		writeToCgi();
		void		readFromCgi();
		void		killCgi();

		Response	buildCgiResponse();

		int			getReadFd() const;
		int			getWriteFd() const;
		pid_t		getPid() const;
		bool		isReadDone() const;
		bool		isWriteDone() const;
		bool		hasError() const;

		// Descriptor cleanup if the object is destroyed prematurely
		void		cleanup();
};