#include "KrPlatform.h"

static void DefaultAssertionHandlerProc(const char *file, int line, const char *proc, const char *msg) {
	TriggerBreakpoint();
}

static void DefaultFatalErrorHandlerProc(const char *msg) {
	TriggerBreakpoint();
}

static void DefaultLoggerProc(void *data, enum LogKind kind, const char *fmt, va_list args) {
	TriggerBreakpoint();
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

void HandleAssertion(const char *file, int line, const char *proc, const char *msg) {
	Thread.OnAssertion(file, line, proc, msg);
}

void FatalError(const char *msg) {
	LogError("Fatal Error: ", msg);
	Thread.OnFatalError(msg);
}
