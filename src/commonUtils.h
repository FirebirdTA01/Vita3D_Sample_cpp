#pragma once

#include "Logger.h"

//Engine specific
#define vitaPrintf Logger::getInstance()->writeLog
//TO DO:
//#define vitaPrintf Logger::getInstance()->applicationMsg
//#define LOG Logger::getInstance()->writeLog

static const char* _padLables[16] = { "SELECT ", "", "", "START ", "UP ","RIGHT ","DOWN ","LEFT ",
	"L ", "R ", "", "", "TRIANGLE ", "CIRCLE ", "CROSS ", "SQUARE " };