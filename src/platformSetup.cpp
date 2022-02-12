#include "platformSetup.h"

#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincon.h>
#endif


void setupLoggingOutput()
{

    bool allAreRedirected = true;
    allAreRedirected &= std::freopen("squint.log", "a", stdout) != nullptr;
    allAreRedirected &= std::freopen("squint.log", "a", stderr) != nullptr;
    if (!allAreRedirected)
    {
        puts("[ERROR] Coudln't set up the logging redirection");
    }
    else
    {
#if defined(NDEBUG) && defined(_WIN32) || defined(_WIN64)
        ShowWindow(GetConsoleWindow(), SW_HIDE);    
#endif
    }
}

void unsetupLoggingOutput()
{
    fflush(stdout);
    fflush(stderr);
}