// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

struct AVDictionary;

class NX_VMS_COMMON_API AvOptions
{
public:
    AvOptions() = default;
    ~AvOptions();
    void set(const char* key, const char* value, int flags = 0);
    void set(const char* key, int64_t value, int flags);
    operator AVDictionary**() { return &m_options; }

private:
    AVDictionary* m_options = nullptr;
};
