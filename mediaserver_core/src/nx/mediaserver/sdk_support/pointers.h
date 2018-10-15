#pragma once

#include <memory>
#include <functional>
#include <plugins/plugin_api.h>

namespace nx::mediaserver::sdk_support {

using SdkObjectDeleter = std::function<void(nxpl::PluginInterface*)>;

void sdkDeleter(nxpl::PluginInterface* pluginInterface);

template <typename RefCountable>
class UniquePtr
{
public:
    UniquePtr() = default;
    UniquePtr(RefCountable* refCountable):
        m_ptr(refCountable, [](nxpl::PluginInterface* ptr) { ptr->releaseRef(); })
    {
    }
    UniquePtr(UniquePtr&& other): m_ptr(std::move(other.m_ptr)) {}
    UniquePtr(const UniquePtr& other) = delete;
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
    bool operator==(nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(const UniquePtr& other) const { return m_ptr != other.m_ptr; }
    bool operator!=(const nullptr_t) const { return m_ptr != nullptr; }


private:
    std::unique_ptr<RefCountable, SdkObjectDeleter> m_ptr;
};

template<typename RefCountable>
class SharedPtr
{
public:
    SharedPtr() = default;
    SharedPtr(RefCountable* refCountable):
        m_ptr(refCountable, [](RefCountable* ptr) { ptr->releaseRef(); })
    {
    }
    SharedPtr(SharedPtr&& other): m_ptr(std::move(other.m_ptr)) {}
    SharedPtr(const SharedPtr& other): m_ptr(other.m_ptr) {};
    ~SharedPtr() = default;

    void reset(RefCountable* ptr = nullptr) noexcept { m_ptr.reset(ptr); }
    RefCountable* release() noexcept { return m_ptr.release(); }
    RefCountable* get() const noexcept { return m_ptr.get(); }
    RefCountable* operator->() const noexcept { return m_ptr.get(); }
    operator bool() const noexcept { return (bool)m_ptr; }

    SharedPtr& operator=(const SharedPtr& other) noexcept
    {
        m_ptr = other.m_ptr;
        return *this;
    };

    SharedPtr& operator=(SharedPtr&& other) noexcept
    {
        m_ptr = std::move(other.m_ptr);
        return *this;
    }

    bool operator==(const SharedPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator==(nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(const SharedPtr& other) const { return m_ptr != other.m_ptr; }
    bool operator!=(const nullptr_t) const { return m_ptr != nullptr; }

private:
    std::shared_ptr<RefCountable> m_ptr;
};

} // namespace nx::mediaserver::sdk_support
