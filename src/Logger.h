#pragma once

//----------------------------------------------
// Logger Class
// Responsible for logging debug information to a file
// Also responsible for calculating FPS as well as
// writing any debug information to the screen
//-----------------------------------------------

#include <fstream>
#include <string>

class Logger
{
protected:
	Logger();
	Logger(Logger const&);
	void operator=(Logger const&);
public:
	~Logger();
	static Logger* getInstance();

	void init();
	void shutdown();
	void writeLog(const char* info, ...);
	void writeLog(std::string info);

private:
	std::ofstream outStream;
};