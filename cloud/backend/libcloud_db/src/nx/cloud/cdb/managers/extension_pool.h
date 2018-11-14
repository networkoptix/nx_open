#pragma once

#include <algorithm>
#include <vector>

namespace nx::cloud::db {

template<typename Extension>
class ExtensionPool
{
public:
    void add(Extension* extension)
    {
        m_extensions.push_back(extension);
    }

    void remove(Extension* extension)
    {
        m_extensions.erase(
            std::remove(m_extensions.begin(), m_extensions.end(), extension),
            m_extensions.end());
    }

    template<typename Func, typename... Args>
    void invoke(Func func, Args... args)
    {
        for (auto extension: m_extensions)
            (extension->*func)(args...);
    }

private:
    std::vector<Extension*> m_extensions;
};

} // namespace nx::cloud::db
