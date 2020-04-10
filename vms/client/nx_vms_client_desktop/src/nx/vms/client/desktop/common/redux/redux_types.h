#pragma once

#include <optional>

namespace nx::vms::client::desktop {

enum class ModificationSource
{
    local,
    remote
};

template<class T>
struct NX_VMS_CLIENT_DESKTOP_API UserEditable
{
    T get() const { return m_user.value_or(m_base); }
    bool hasUser() const { return m_user.has_value(); }
    void setUser(T value) { m_user = value; }
    T getBase() const { return m_base; }
    void setBase(T value) { m_base = value; }
    void resetUser() { m_user = {}; }

    T operator()() const { return get(); }

private:
    T m_base = T();
    std::optional<T> m_user;
};

template<class T>
struct NX_VMS_CLIENT_DESKTOP_API UserEditableMultiple
{
    bool hasValue() const { return m_user || m_base; }
    T get() const { return m_user.value_or(*m_base); }
    T valueOr(T value) const { return m_user.value_or(m_base.value_or(value)); }
    void setUser(T value) { m_user = value; }
    void setBase(T value) { m_base = value; }
    void resetUser() { m_user = {}; }
    void resetBase() { m_base = {}; }

    T operator()() const { return get(); }
    operator std::optional<T>() const { return m_user ? m_user : m_base; }

    bool equals(T value) const { return hasValue() && get() == value; }

    static UserEditableMultiple<T> fromValue(const T& value)
    {
        UserEditableMultiple<T> result;
        result.setBase(value);
        return result;
    }

private:
    std::optional<T> m_base;
    std::optional<T> m_user;
};

enum class CombinedValue
{
    None,
    Some,
    All
};

} // namespace nx::vms::client::desktop
