#pragma once

#include <dlfcn.h>

class Library
{
public:
    Library(const char* path);
    ~Library();
    bool isOpened() const;

    template<typename FuncPtr>
    FuncPtr symbol(const char* name)
    {
        if (m_handle == nullptr)
            return nullptr;

        return (FuncPtr)dlsym(m_handle, name);
    }

private:
    void* m_handle;
};