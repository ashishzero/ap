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
	.Random        = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL },
	.Logger.Handle = DefaultLoggerProc,
	.OnAssertion   = DefaultAssertionHandlerProc,
	.OnFatalError  = DefaultFatalErrorHandlerProc
};

void LogEx(enum LogKind kind, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.Logger.Handle(Thread.Logger.Data, kind, fmt, args);
	va_end(args);
}

void LogTrace(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.Logger.Handle(Thread.Logger.Data, LogKind_Trace, fmt, args);
	va_end(args);
}

void LogWarning(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.Logger.Handle(Thread.Logger.Data, LogKind_Warning, fmt, args);
	va_end(args);
}

void LogError(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Thread.Logger.Handle(Thread.Logger.Data, LogKind_Error, fmt, args);
	va_end(args);
}

void HandleAssertion(const char *file, int line) {
	Thread.OnAssertion(file, line);
}

void FatalError() {
	Thread.OnFatalError();
}
