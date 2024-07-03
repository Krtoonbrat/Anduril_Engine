//
// Created by kjhughes4 on 10/25/2023.
//

#include "misc.h"

#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#if _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601  // Force to include needed API prototypes
#endif

#include <windows.h>
// The needed Windows API for processor groups could be missed from old Windows
// versions, so instead of calling them directly (forcing the linker to resolve
// the calls at compile time), try to load them at runtime. To do this we need
// first to define the corresponding function pointers.
extern "C" {
using fun1_t = bool (*)(LOGICAL_PROCESSOR_RELATIONSHIP,
                        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,
                        PDWORD);
using fun2_t = bool (*)(USHORT, PGROUP_AFFINITY);
using fun3_t = bool (*)(HANDLE, CONST GROUP_AFFINITY*, PGROUP_AFFINITY);
using fun4_t = bool (*)(USHORT, PGROUP_AFFINITY, USHORT, PUSHORT);
using fun5_t = WORD (*)();
using fun6_t = bool (*)(HANDLE, DWORD, PHANDLE);
using fun7_t = bool (*)(LPCSTR, LPCSTR, PLUID);
using fun8_t = bool (*)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
}
#endif

// from stockfish to figure out if we can use posix_memalign
#if defined(__APPLE__) || defined(__ANDROID__) || defined(__OpenBSD__) \
  || (defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC) && !defined(_WIN32)) \
  || defined(__e2k__)
#define POSIXALIGNEDALLOC
    #include <stdlib.h>
#endif

namespace NNUE {

    char nnue_path[256] = "../egbdll/nets/nn-c157e0a5755b.nnue";


    void LoadNNUE() {
        nnue_init(nnue_path);
    }

    int NNUE_evaluate(int player, int *pieces, int *squares) {
        return nnue_evaluate(player, pieces, squares);
    }

    int NNUE_incremental(int player, int *pieces, int *squares, NNUEdata** data) {
        return nnue_evaluate_incremental(player, pieces, squares, data);
    }

} // namespace NNUE

// mostly all based on stockfish for the memory allocation stuff
void* std_aligned_alloc(size_t alignment, size_t size) {
#if defined(POSIXALIGNEDALLOC)
    void* ptr;
    return posix_memalign(&ptr, alignment, size) ? nullptr : ptr;
#elif defined(_WIN32) && !defined(_M_ARM) && !defined(_M_ARM64)
    return _mm_malloc(size, alignment);
#elif defined(_win32)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void std_aligned_free(void* ptr) {
#if defined(POSIXALIGNEDALLOC)
    free(ptr);
#elif defined(_WIN32) && !defined(_M_ARM) && !defined(_M_ARM64)
    _mm_free(ptr);
#elif defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// large page allocation from stockfish 16
// why must windows make this so disgusting?
#if defined(_WIN32)

static void* aligned_large_pages_alloc_windows([[maybe_unused]] size_t allocSize) {

#if !defined(_WIN64)
    return nullptr;
#else

    HANDLE hProcessToken{};
    LUID   luid{};
    void*  mem = nullptr;

    const size_t largePageSize = GetLargePageMinimum();
    if (!largePageSize)
        return nullptr;

    // Dynamically link OpenProcessToken, LookupPrivilegeValue and AdjustTokenPrivileges

    HMODULE hAdvapi32 = GetModuleHandle(TEXT("advapi32.dll"));

    if (!hAdvapi32)
        hAdvapi32 = LoadLibrary(TEXT("advapi32.dll"));

    auto fun6 = fun6_t((void (*)()) GetProcAddress(hAdvapi32, "OpenProcessToken"));
    if (!fun6)
        return nullptr;
    auto fun7 = fun7_t((void (*)()) GetProcAddress(hAdvapi32, "LookupPrivilegeValueA"));
    if (!fun7)
        return nullptr;
    auto fun8 = fun8_t((void (*)()) GetProcAddress(hAdvapi32, "AdjustTokenPrivileges"));
    if (!fun8)
        return nullptr;

    // We need SeLockMemoryPrivilege, so try to enable it for the process
    if (!fun6(  // OpenProcessToken()
            GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken))
        return nullptr;

    if (fun7(  // LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &luid)
            nullptr, "SeLockMemoryPrivilege", &luid))
    {
        TOKEN_PRIVILEGES tp{};
        TOKEN_PRIVILEGES prevTp{};
        DWORD            prevTpLen = 0;

        tp.PrivilegeCount           = 1;
        tp.Privileges[0].Luid       = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        // Try to enable SeLockMemoryPrivilege. Note that even if AdjustTokenPrivileges() succeeds,
        // we still need to query GetLastError() to ensure that the privileges were actually obtained.
        if (fun8(  // AdjustTokenPrivileges()
                hProcessToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &prevTp, &prevTpLen)
            && GetLastError() == ERROR_SUCCESS)
        {
            // Round up size to full pages and allocate
            allocSize = (allocSize + largePageSize - 1) & ~size_t(largePageSize - 1);
            mem       = VirtualAlloc(nullptr, allocSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                     PAGE_READWRITE);

            // Privilege no longer needed, restore previous state
            fun8(  // AdjustTokenPrivileges ()
                    hProcessToken, FALSE, &prevTp, 0, nullptr, nullptr);
        }
    }

    CloseHandle(hProcessToken);

    return mem;

#endif
}

void* aligned_large_pages_alloc(size_t size) {

    // try large pages first
    void* mem = aligned_large_pages_alloc_windows(size);

    // regular alignment if large pages fail
    if (!mem) {
        mem = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }

    return mem;

}

#else

void* aligned_large_pages_alloc(size_t size) {

#if defined(__linux__)
    constexpr size_t alignment = 2 * 1024 * 1024;
#else
    constexpr size_t alignment = 4096;
#endif

    // round to multiples of alignment
    size = ((size + alignment - 1) / alignment) * alignment;
    void* mem = std_aligned_alloc(alignment, size);
    return mem;

}

#endif

// free the memory
#if defined(_WIN32)

void aligned_large_pages_free(void* mem) {

    if (mem && !VirtualFree(mem, 0, MEM_RELEASE))
    {
        DWORD err = GetLastError();
        std::cerr << "Failed to free large page memory. Error code: 0x" << std::hex << err
                  << std::dec << std::endl;
        exit(EXIT_FAILURE);
    }
}

#else

void aligned_large_pages_free(void* mem) { std_aligned_free(mem); }

#endif

inline uint64_t byteSwap(uint64_t value) {
#if defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    return __builtin_bswap64(value);
#endif
}