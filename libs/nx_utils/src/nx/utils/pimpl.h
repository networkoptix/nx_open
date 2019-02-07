#pragma once

#include <memory>

namespace nx::utils {

// Const-aware pointer to a private implementation.
template<typename T>
class PImpl final
{
public:
    explicit PImpl(T* ptr): m_ptr(ptr) {}
    ~PImpl() = default;

    PImpl() = delete;
    PImpl(nullptr_t) = delete;
    PImpl(const PImpl&) = delete;
    PImpl(PImpl&&) = delete;
    PImpl& operator=(const PImpl&) = delete;
    PImpl& operator=(PImpl&&) = delete;

    const T* get() const { return m_ptr.get(); }
    T* get() { return m_ptr.get(); }

    const T* data() const { return get(); }
    T* data() { return get(); }

    const T* operator->() const { return get(); }
    T* operator->() { return get(); }

    const T& operator*() const { return *get(); }
    T& operator*() { return *get(); }

private:
    const std::unique_ptr<T> m_ptr;
};

} // namespace nx::utils
