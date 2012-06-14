#ifndef _COMMON_COMPATIBILITY_H
#define _COMMON_COMPATIBILITY_H

// Presense of an entry in global table means
// that component of ver1 is compatible (or has compatibility mode)
// with EVERY component of ver2
struct CompatibilityInfo {
    const char* ver1;
    const char* component;
    const char* ver2;
};

#endif // _COMMON_COMPATIBILITY_H
