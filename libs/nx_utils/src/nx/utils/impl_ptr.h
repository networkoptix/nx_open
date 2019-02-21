#pragma once

#include <memory>

namespace nx::utils {

// Const-aware pointer to a private implementation.
template<typename T>
class ImplPtr final
{
public:
    explicit ImplPtr(T* ptr): m_ptr(ptr) {}
    ~ImplPtr() = default;

    ImplPtr() = delete;
    ImplPtr(std::nullptr_t) = delete;
    ImplPtr(const ImplPtr&) = delete;
    ImplPtr(ImplPtr&&) = delete;
    ImplPtr& operator=(const ImplPtr&) = delete;
    ImplPtr& operator=(ImplPtr&&) = delete;

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
