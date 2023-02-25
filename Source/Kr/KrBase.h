#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <float.h>
#include <uchar.h>
#include <limits.h>

#if defined(__clang__) || defined(__ibmxl__)
#define M_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define M_COMPILER_CL 1
#elif defined(__GNUC__)
#define M_COMPILER_GCC 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
#define M_COMPILER_MINGW 1
#elif defined(__INTEL_COMPILER)
#define M_COMPILER_INTEL 1
#else
#error Missing Compiler detection
#endif

#if !defined(M_COMPILER_CLANG)
#define M_COMPILER_CLANG 0
#endif
#if !defined(M_COMPILER_CL)
#define M_COMPILER_CL 0
#endif
#if !defined(M_COMPILER_GCC)
#define M_COMPILER_GCC 0
#endif
#if !defined(M_COMPILER_INTEL)
#define M_COMPILER_INTEL 0
#endif

#if defined(__ANDROID__) || defined(__ANDROID_API__)
#define M_PLATFORM_ANDROID 1
#define __PLATFORM__ "Andriod"
#elif defined(__gnu_linux__) || defined(__linux__) || defined(linux) || defined(__linux)
#define M_PLATFORM_LINUX 1
#define __PLATFORM__ "linux"
#elif defined(macintosh) || defined(Macintosh)
#define M_PLATFORM_MAC 1
#define __PLATFORM__ "Mac"
#elif defined(__APPLE__) && defined(__MACH__)
#define M_PLATFORM_MAC 1
#define __PLATFORM__ "Mac"
#elif defined(__APPLE__)
#define M_PLATFORM_IOS 1
#define __PLATFORM__ "iOS"
#elif defined(_WIN64) || defined(_WIN32)
#define M_PLATFORM_WINDOWS 1
#define __PLATFORM__ "Windows"
#else
#error Missing Operating System Detection
#endif

#if !defined(M_PLATFORM_ANDROID)
#define M_PLATFORM_ANDROID 0
#endif
#if !defined(M_PLATFORM_LINUX)
#define M_PLATFORM_LINUX 0
#endif
#if !defined(M_PLATFORM_MAC)
#define M_PLATFORM_MAC 0
#endif
#if !defined(M_PLATFORM_IOS)
#define M_PLATFORM_IOS 0
#endif
#if !defined(M_PLATFORM_WINDOWS)
#define M_PLATFORM_WINDOWS 0
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64) || \
    defined(_M_X64)
#define M_ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
#define M_ARCH_X86 1
#elif defined(__arm__) || defined(__thumb__) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARMT)
#define M_ARCH_ARM 1
#elif defined(__aarch64__)
#define M_ARCH_ARM64 1
#else
#error Missing Architecture Identification
#endif

#if !defined(M_ARCH_X64)
#define M_ARCH_X64 0
#endif
#if !defined(M_ARCH_X86)
#define M_ARCH_X86 0
#endif
#if !defined(M_ARCH_ARM)
#define M_ARCH_ARM 0
#endif
#if !defined(M_ARCH_ARM64)
#define M_ARCH_ARM64 0
#endif

#if !defined(M_ENDIAN_LITTLE) && !defined(M_ENDIAN_BIG)
#define M_ENDIAN_LITTLE 1
#define M_ENDIAN_BIG    0
#else
#define M_ENDIAN_LITTLE 0
#define M_ENDIAN_BIG    1
#endif

#if defined(__GNUC__)
#define __PROCEDURE__ __FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define __PROCEDURE__ __PRETTY_PROCEDURE__
#elif defined(__FUNCSIG__)
#define __PROCEDURE__ __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define __PROCEDURE__ __PROCEDURE__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define __PROCEDURE__ __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define __PROCEDURE__ __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define __PROCEDURE__ __func__
#elif defined(_MSC_VER)
#define __PROCEDURE__ __FUNCSIG__
#else
#define __PROCEDURE__ "_unknown_"
#endif

#if defined(HAVE_SIGNAL_H) && !defined(__WATCOMC__)
#include <signal.h> // raise()
#endif

#ifndef TriggerBreakpoint
#if defined(_MSC_VER)
#define TriggerBreakpoint() __debugbreak()
#elif ((!defined(__NACL__)) && ((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))))
#define TriggerBreakpoint() __asm__ __volatile__("int $3\n\t")
#elif defined(__386__) && defined(__WATCOMC__)
#define TriggerBreakpoint() _asm { int 0x03 }
#elif defined(HAVE_SIGNAL_H) && !defined(__WATCOMC__)
#define TriggerBreakpoint() raise(SIGTRAP)
#else
#define TriggerBreakpoint() ((int *)0) = 0
#endif
#endif

#ifndef thread_local
#if defined(M_COMPILER_CL)
#define thread_local __declspec( thread )
#else
#define thread_local _Thread_local
#endif
#endif

#if !defined(M_BUILD_DEBUG) && !defined(M_BUILD_DEVELOP) && !defined(M_BUILD_RELEASE)
#if defined(_DEBUG) || defined(DEBUG)
#define M_BUILD_DEBUG
#elif defined(NDEBUG)
#define M_BUILD_RELEASE
#else
#define M_BUILD_DEBUG
#endif
#endif

#if defined(M_BUILD_DEBUG) || defined(M_BUILD_DEVELOP)
#define DebugTriggerBreakpoint TriggerBreakpoint
#else
#define DebugTriggerBreakpoint()
#endif

#ifdef __GNUC__
_Noreturn static inline __attribute__((always_inline)) void Unreachable() { DebugTriggerBreakpoint(); __builtin_unreachable(); }
#elif defined(_MSC_VER)
_Noreturn __forceinline void Unreachable(void) { DebugTriggerBreakpoint(); __assume(false); }
#else
static inline void Unreachable(void) { TriggerBreakpoint(); }
#endif

#define NoDefaultCase() default: Unreachable(); break

_Noreturn static inline void Unimplemented(void) { TriggerBreakpoint(); }

//
//
//

typedef uint32_t              uint;
typedef float                 real;
typedef uint8_t               byte;
typedef uint8_t               u8;
typedef uint16_t              u16;
typedef uint32_t              u32;
typedef uint64_t              u64;
typedef int8_t                i8;
typedef int16_t               i16;
typedef int32_t               i32;
typedef int64_t               i64;
typedef float                 r32;
typedef double                r64;
typedef size_t                umem;
typedef ptrdiff_t             imem;

#define REAL32_MIN            FLT_MIN
#define REAL32_MAX            FLT_MAX
#define REAL64_MIN            DBL_MIN
#define REAL64_MAX            DBL_MAX
#define REAL_MIN              REAL32_MIN
#define REAL_MAX              REAL32_MAX

#define nullptr               ((void *)0)
#define ArrayCount(a)         (sizeof(a) / sizeof((a)[0]))

#define Min(a, b)             (((a) < (b))?(a):(b))
#define Max(a, b)             (((a) > (b))?(a):(b))
#define Clamp(a, b, v)        Min(b, Max(a, v))
#define IsInRange(a, b, v)    ((v) >= (a) && (v) <= (b))
#define IsPower2(value)       (((value) != 0) && ((value) & ((value)-1)) == 0)
#define AlignUp2(x, p)        (((x) + (p)-1) & ~((p)-1))
#define AlignDown2(x, p)      ((x) & ~((p)-1))

#define ByteSwap16(a)         ((((a)&0x00FF) << 8) | (((a)&0xFF00) >> 8))
#define ByteSwap32(a)         ((((a)&0x000000FF) << 24) | (((a)&0x0000FF00) << 8) | \
                              (((a)&0x00FF0000) >> 8) | (((a)&0xFF000000) >> 24))
#define ByteSwap64(a)         ((((a)&0x00000000000000FFULL) << 56) | (((a)&0x000000000000FF00ULL) << 40) | \
                              (((a)&0x0000000000FF0000ULL) << 24) | (((a)&0x00000000FF000000ULL) << 8) | \
                              (((a)&0x000000FF00000000ULL) >> 8) | (((a)&0x0000FF0000000000ULL) >> 24) | \
                              (((a)&0x00FF000000000000ULL) >> 40) | (((a)&0xFF00000000000000ULL) >> 56))

#if M_ENDIAN_LITTLE
#define ByteSwap16Little(a)   (a)
#define ByteSwap32Little(a)   (a)
#define ByteSwap64Little(a)   (a)
#define ByteSwap16Big(a)      ByteSwap16(a)
#define ByteSwap32Big(a)      ByteSwap32(a)
#define ByteSwap64Big(a)      ByteSwap64(a)
#else
#define ByteSwap16Little(a)   ByteSwap16(a)
#define ByteSwap32Little(a)   ByteSwap32(a)
#define ByteSwap64Little(a)   ByteSwap64(a)
#define ByteSwap16Big(a)      (a)
#define ByteSwap32Big(a)      (a)
#define ByteSwap64Big(a)      (a)
#endif

#define KB(n)                 ((n) * 1024ULL)
#define MB(n)                 (KB(n) * 1024ULL)
#define GB(n)                 (MB(n) * 1024ULL)

#if defined(M_BUILD_DEBUG) || defined(M_BUILD_DEVELOP) || defined(M_FORCE_ASSERTIONS)
#define Assert(x)             do {                                        \
                                if (!(x)) {                               \
                                    HandleAssertion(__FILE__, __LINE__);  \
                                    TriggerBreakpoint();                  \
                                }                                         \
                              } while (0)
#else
#define Assert(x)
#endif

typedef struct RandomSource {
    u64 state;
    u64 inc;
} RandomSource;

typedef enum LogKind {
    LogKind_Trace, LogKind_Info, LogKind_Warning, LogKind_Error
} LogKind;

typedef struct Logger {
    void (*proc)(void *, enum LogKind, const char *, va_list);
    void * data;
} Logger;

typedef struct AssertionHandler {
    void (*proc)(const char *, int);
} AssertionHandler;

typedef struct FatalErrorHandler {
    void (*proc)();
} FatalErrorHandler;

struct ThreadContext {
    RandomSource      random;
    Logger            logger;
    AssertionHandler  assertion_handler;
    FatalErrorHandler fatal_error_handler;
};

extern thread_local struct ThreadContext Thread;

void LogEx(enum LogKind kind, const char *fmt, ...);
void LogTrace(const char *fmt, ...);
void LogWarning(const char *fmt, ...);
void LogError(const char *fmt, ...);
void HandleAssertion(const char *file, int line);
void FatalError();
