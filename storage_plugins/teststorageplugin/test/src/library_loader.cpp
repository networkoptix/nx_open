#include "library_loader.h"


Library::Library(const char* path) : m_handle(dlopen(path, RTLD_LAZY))
{
}

Library::~Library()
{
    if (m_handle != nullptr)
        dlclose(m_handle);
}

bool Library::isOpened() const
{
    return m_handle != nullptr;
}