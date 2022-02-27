// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

namespace nx::utils {

/**
 * Const-aware pointer to a private implementation.
 *
 * The main problem with "const QScopedPointer<Private> d" - it's not const-aware,
 * i.e. we can assign "d->someField = someValue" from a const method.
 * This class is designed to overcome that problem.
 *
 * It should not be const itself, having no copy/move assignment operators it cannot be reassigned.
 * There is no default constructor as well, so it's not possible to forget to initialize it.
 *
 * Typical usage inside a class declaration:
 *
 * private:
 *     class Private;
 *     nx::utils::ImplPtr<Private> d;
 */
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
