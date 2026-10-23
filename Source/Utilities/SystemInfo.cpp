#include "SystemInfo.h"

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <psapi.h>
 #pragma comment(lib, "psapi.lib")
#elif defined(__linux__)
 #include <cstdio>
 #include <unistd.h>
#endif

namespace amp
{
    std::size_t processMemoryBytes() noexcept
    {
       #if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS pmc {};
        if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)))
            return static_cast<std::size_t> (pmc.WorkingSetSize);
        return 0;
       #elif defined(__linux__)
        long pages = 0, resident = 0;
        if (FILE* f = std::fopen ("/proc/self/statm", "r"))
        {
            if (std::fscanf (f, "%ld %ld", &pages, &resident) != 2)
                resident = 0;
            std::fclose (f);
        }
        return static_cast<std::size_t> (resident) * static_cast<std::size_t> (sysconf (_SC_PAGESIZE));
       #else
        return 0;
       #endif
    }
}
