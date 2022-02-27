// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>
#include <type_traits>
#include <memory>
#include <vector>

namespace nx::utils {

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
class NX_UTILS_API ScopedGarbageCollector
{
public:
    ScopedGarbageCollector() = default;

    ScopedGarbageCollector(ScopedGarbageCollector&&) = default;
    ScopedGarbageCollector& operator=(ScopedGarbageCollector&&) = default;

    ScopedGarbageCollector(const ScopedGarbageCollector&) = delete;
    ScopedGarbageCollector& operator=(const ScopedGarbageCollector&) = delete;

    ~ScopedGarbageCollector();

    template <typename Value, typename... Args>
    Value* create(Args&&... args)
    {
        auto ptr = std::make_unique<Value>(std::forward<Args>(args)...);
        m_ptrs.emplace_back(ptr.get(), [](void *ptr) { delete static_cast<Value*>(ptr); });
        return ptr.release();
    }

    template <typename... Guard, typename Value,
        std::enable_if_t<sizeof...(Guard) == 0>* = nullptr>
    std::remove_cvref_t<Value>* create(Value&& value)
    {
        return create<std::remove_cvref_t<Value>>(std::forward<Value>(value));
    }

    void destroyAll();

private:
    std::vector<std::unique_ptr<void, void(*)(void*)>> m_ptrs;
};

} // namespace nx::utils
