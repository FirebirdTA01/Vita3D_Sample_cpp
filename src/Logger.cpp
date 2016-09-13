#include "Logger.h"
//#include "text.h"

#include <stdarg.h>
#include <stdlib.h>

Logger::Logger()
{

}

Logger::~Logger()
{
	//make sure the stream is closed before destroying Logger
	if (outStream.is_open())
		outStream.close();
}

Logger* Logger::getInstance()
{
	static Logger instance;
	return &instance;
}

void Logger::init()
{
	outStream.open("ux0:/graphicsTestLog.txt");

	writeLog("Initializing Logger\n");
	//textInit();
	writeLog("Logger Initialized\n");
}

void Logger::shutdown()
{
	writeLog("Shutting-down Logger\n");
	outStream.close();
}

void Logger::writeLog(const char* info, ...)
{
	char buf[512];

	va_list args;
	va_start(args, info);
	int result = vsnprintf(buf, sizeof(buf), info, args);
	outStream << buf;
	va_end(args);
}

void Logger::writeLog(std::string info)
{
	outStream << info;
}