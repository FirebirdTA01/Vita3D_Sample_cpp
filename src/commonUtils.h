#pragma once

#include "Logger.h"

//Engine specific
#define EMPrintf Logger::getInstance()->writeLog
//TO DO:
//#define EMPrintf Logger::getInstance()->applicationMsg
//#define LOG Logger::getInstance()->writeLog

static const char* _padLables[16] = { "SELECT ", "", "", "START ", "UP ","RIGHT ","DOWN ","LEFT ",
	"L ", "R ", "", "", "TRIANGLE ", "CIRCLE ", "CROSS ", "SQUARE " };