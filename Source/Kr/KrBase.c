#include "krBase.h"

#include <stdlib.h>
#include <stdio.h>

static void DefaultAssertionHandlerProc(const char *file, int line) {
}

static void DefaultFatalErrorHandlerProc() {
	exit(1);
}

static void DefaultLoggerProc(void *data, enum LogKind kind, const char *fmt, va_list args) {
	FILE *out = kind == LogKind_Error ? stderr : stdout;
	vfprintf(out, fmt, args);
}

thread_local struct ThreadContext Thread = {
	.random                   = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL },
	.logger.proc              = DefaultLoggerProc,
	.assertion_handler.proc   = DefaultAssertionHandlerProc,
	.fatal_error_handler.proc = DefaultFatalErrorHandlerProc,
};

void LogEx(enum LogKind kind, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.logger.proc(Thread.logger.data, kind, fmt, args);
	va_end(args);
}

void LogTrace(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.logger.proc(Thread.logger.data, LogKind_Trace, fmt, args);
	va_end(args);
}

void LogWarning(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.logger.proc(Thread.logger.data, LogKind_Warning, fmt, args);
	va_end(args);
}

void LogError(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.logger.proc(Thread.logger.data, LogKind_Error, fmt, args);
	va_end(args);
}

void HandleAssertion(const char *file, int line) {
	Thread.assertion_handler.proc(file, line);
}

void FatalError() {
	Thread.fatal_error_handler.proc();
}
