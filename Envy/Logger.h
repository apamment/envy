#pragma once

#include <string>

#define LOG_ERROR 0
#define LOG_INFO  1
#define LOG_DEBUG 2

class Logger
{
public:
	Logger();
	void load(std::string filename);
	void log(int severity, const char* fmt, ...);

private:
	bool is_loaded;
	std::string logfile;
};

