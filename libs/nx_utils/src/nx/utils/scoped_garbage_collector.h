#pragma once

#include <algorithm>
#include <memory>
#include <vector>

namespace nx {
namespace utils {

/**
* Scoped Garbage Collector.
* Declare
* Usage:
* 1. Declare garbage collector, e.g.
*     <code>ScopedGarbageCollector gc;</code>
* 2. Create objects with "create" member-function, e.g.
* <pre><code>
*     int* i = gc.create<int>;
*     std::string* s1 = gc.create<std::string>("Hello!");
*     auto s2 = gc.create<std::string>("Bye!");
*     MyClass* myClass = gc.create<MyClass>(100, 200, "Ok");
* </code></pre>
* 3. Objects will be deleted when gc leaves it scope.
* 4. Information: objects are deleted in reversed order: last created id destroyed first.
*/
class ScopedGarbageCollector final
{
    class BaseDeleter
    {
    public:
        virtual ~BaseDeleter() = default;
    };

    template<class T>
    class Deleter: public BaseDeleter
    {
        T* m_object;
    public:
        Deleter(T* object) noexcept : m_object(object) {}
        virtual ~Deleter() override { delete m_object; }
    };

    std::vector<BaseDeleter*> m_deleters;

public:
    ~ScopedGarbageCollector()
    {
        std::for_each(m_deleters.rbegin(), m_deleters.rend(), [](BaseDeleter* d) { delete d; });
    }

    template<class T, class ...Args>
    T* create(Args&& ... args)
    {
        // auto_ptr provides exception safety if code below throws exception.
        auto object = std::make_unique<T>(std::forward<Args>(args)...);

        // Separate push_back call provides exception safety if push_back fails.
        m_deleters.push_back(nullptr);
        m_deleters.back() = new Deleter<T>(object.get());

        return object.release();
    }
};

} // namespace utils
} // namespace nx
