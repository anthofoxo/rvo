// This file contains only `#if 0` blocks showcasing potential features to be implemented in future


// This block demonstrates how to can catch stack overflows at runtime. Windows only
#if 0
#include <windows.h>

// Invoking this function will trigger an overflow
void StackOverFlow() { StackOverFlow(); }

// Msvc Exception Filter
DWORD ExceptionFilter(EXCEPTION_POINTERS* pointers, DWORD dwException) { return EXCEPTION_EXECUTE_HANDLER; }

// MSVC specific try catch block
__try { StackOverFlow(); }
__except (ExceptionFilter(GetExceptionInformation(), GetExceptionCode())) {}
#endif