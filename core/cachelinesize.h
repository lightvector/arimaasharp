
// Author: Nick Strupat
// Date: October 29, 2010
// Returns the cache line size (in bytes) of the processor, or 0 on failure
//
// Minor modifications by David Wu
// http://stackoverflow.com/questions/794632/programmatically-get-the-cache-line-size

#ifndef GET_CACHE_LINE_SIZE_H_INCLUDED
#define GET_CACHE_LINE_SIZE_H_INCLUDED

#include <stddef.h>

namespace CacheLineSize
{
  size_t getCacheLineSize();
}

#if defined(__APPLE__)

#include <sys/sysctl.h>
inline size_t CacheLineSize::getCacheLineSize() {
    size_t line_size = 0;
    size_t sizeof_line_size = sizeof(line_size);
    sysctlbyname("hw.cachelinesize", &line_size, &sizeof_line_size, 0, 0);
    return line_size;
}

#elif defined(_WIN32)

#include <stdlib.h>
#include <windows.h>
inline size_t CacheLineSize::getCacheLineSize() {
    size_t line_size = 0;
    DWORD buffer_size = 0;
    DWORD i = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION * buffer = 0;

    GetLogicalProcessorInformation(0, &buffer_size);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(buffer_size);
    GetLogicalProcessorInformation(&buffer[0], &buffer_size);

    for (i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
        if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
            line_size = buffer[i].Cache.LineSize;
            break;
        }
    }

    free(buffer);
    return line_size;
}

#elif defined(__unix)

#include <stdio.h>
inline size_t CacheLineSize::getCacheLineSize() {
    FILE * p = 0;
    p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (p) {
        int i = 0;
        int scanned = fscanf(p, "%d", &i);
        fclose(p);
        if(scanned > 0)
          return (unsigned)i;
    }
    return 0;
}

#else
#error Unrecognized platform
#endif

#endif //GET_CACHE_LINE_SIZE_H_INCLUDED
