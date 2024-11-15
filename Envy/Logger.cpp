#include <stdarg.h>
#include <ctime>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "Logger.h"

Logger::Logger()
{
	is_loaded = false;
}

void Logger::load(std::string filename)
{
	logfile = filename;
	is_loaded = true;
}

static inline char hex_char(uint8_t c) {
	if (c >= 10) {
		return (c - 10) + 'A';
	}
	return c + '0';
}

void Logger::log(int severity, const char* fmt, ...)
{
	static const char* sev[] = { "ERROR", " INFO", "DEBUG" };
	char buffer[2048];
	va_list args;
	time_t now = time(NULL);
	struct tm time_tm;
	if (!is_loaded) return;
    int pid;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof buffer, fmt, args);
	va_end(args);

#if _MSC_VER
	localtime_s(&time_tm, &now);
#else
	localtime_r(&now, &time_tm);
#endif

#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	std::stringstream ss;

	for (size_t i=0; i<strlen(buffer); i++) {
		if (!isprint(buffer[i])) {
			ss << "\\0x" << hex_char(buffer[i] >> 8) << hex_char(buffer[i] & 0x0F);
		} else {
			ss << buffer[i];
		}
	}

	FILE* fptr = fopen(logfile.c_str(), "a");
	if (fptr) {
		fprintf(fptr, "%04d-%02d-%02d %02d:%02d:%02d [%d] %s: %s\n", time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec, pid, sev[severity], ss.str().c_str());
		fclose(fptr);
	}
}
