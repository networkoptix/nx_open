#pragma once

#include <memory>
#include <functional>
#include <plugins/plugin_api.h>

// TODO: #mshevchenko: Move to namespace common.
namespace nx::vms::server::sdk_support {

using SdkObjectDeleter = std::function<void(const nxpl::PluginInterface*)>;

template <typename RefCountable>
class UniquePtr
{
public:
    UniquePtr(RefCountable* refCountable):
        m_ptr(
            refCountable,
            [](const nxpl::PluginInterface* ptr)
            {
                // TODO: #dmishin Remove const_cast when releaseRef becomes const.
                if (ptr)
                    const_cast<nxpl::PluginInterface*>(ptr)->releaseRef();
            })
    {
    }

    UniquePtr(UniquePtr&& other): m_ptr(std::move(other.m_ptr)) {}
    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr(): UniquePtr(nullptr) {}
    ~UniquePtr() = default;

    void reset(RefCountable* ptr = nullptr) noexcept { m_ptr.reset(ptr); }
    RefCountable* release() noexcept { return m_ptr.release(); }
    RefCountable* get() const noexcept { return m_ptr.get();  }
    RefCountable* operator->() const noexcept { return m_ptr.get(); }
    operator bool() const noexcept { return (bool) m_ptr; }

    UniquePtr& operator=(const UniquePtr& other) = delete;
    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        m_ptr = std::move(other.m_ptr);
        return *this;
    }

    bool operator==(const UniquePtr& other) const { return m_ptr == other.m_ptr; }
    bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(const UniquePtr& other) const { return m_ptr != other.m_ptr; }
    bool operator!=(const std::nullptr_t) const { return m_ptr != nullptr; }

private:
    std::unique_ptr<RefCountable, SdkObjectDeleter> m_ptr;
};

template<typename RefCountable>
class SharedPtr
{
public:
    SharedPtr(RefCountable* refCountable):
        m_ptr(
            refCountable,
            [](const nxpl::PluginInterface* ptr)
            {
                // TODO: #dmishin Remove const_cast when releaseRef becomes const.
                if (ptr)
                    const_cast<nxpl::PluginInterface*>(ptr)->releaseRef();
            })
    {
    }
    SharedPtr(SharedPtr&& other): m_ptr(std::move(other.m_ptr)) {}
    SharedPtr(const SharedPtr& other): m_ptr(other.m_ptr) {}
    SharedPtr(): SharedPtr(nullptr) {}
    ~SharedPtr() = default;

    void reset(RefCountable* ptr = nullptr) noexcept { m_ptr.reset(ptr); }
    RefCountable* release() noexcept { return m_ptr.release(); }
    RefCountable* get() const noexcept { return m_ptr.get(); }
    RefCountable* operator->() const noexcept { return m_ptr.get(); }
    operator bool() const noexcept { return (bool) m_ptr; }

    SharedPtr& operator=(const SharedPtr& other) noexcept
    {
        m_ptr = other.m_ptr;
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept
    {
        m_ptr = std::move(other.m_ptr);
        return *this;
    }

    bool operator==(const SharedPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(const SharedPtr& other) const { return m_ptr != other.m_ptr; }
    bool operator!=(const std::nullptr_t) const { return m_ptr != nullptr; }

private:
    std::shared_ptr<RefCountable> m_ptr;
};

} // namespace nx::vms::server::sdk_support
