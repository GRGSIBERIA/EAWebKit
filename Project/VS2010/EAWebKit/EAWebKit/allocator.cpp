#include <EASTL\allocator.h>

void* operator new[](size_t size, const char* pName, int flags,
    unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}
void* operator new[](size_t size, size_t alignment, size_t alignmentOffset,
    const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    // doesn't support alignment
    return malloc(size);
}