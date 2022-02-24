// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

enum class ModificationSource
{
    local,
    remote
};

/**
 * Utility class to implement state properties, which can be modified both locally end remotely.
 * Remote value (base) must exist, local value (user-provided) may be absent. Property will be
 * displayed using user value (if present), base otherwise.
 *
 * Example: Camera name in Camera settings. Until user modified it, we display base value. When it
 * is changed on the Server side, we display changes. When user enters new local value, it is
 * displayed no matter what occurs remotely.
 */
template<class T>
struct UserEditable
{
    T get() const { return m_user.value_or(m_base); }
    T operator()() const { return get(); }

    T getBase() const { return m_base; }
    void setBase(T value) { m_base = value; }

    bool hasUser() const { return m_user.has_value(); }
    void setUser(T value) { m_user = value; }
    void resetUser() { m_user = std::nullopt; }

private:
    T m_base = T{};
    std::optional<T> m_user;
};

/**
 * Utility class to implement state properties, which can be modified both locally end remotely.
 * Intended for using with multiple sources, so remote value (base) exists when it is the same for
 * the all sources and is absent otherwise. Property will be displayed using user value (if
 * present), base otherwise.
 *
 * Example: "Recording enabled" flag in Camera settings. Until user modified it, we display base
 * value: checked if recording is enabled on all Cameras, unchecked when disabled everywhere,
 * tristate when differs (empty base value). When it is changed on the Server side, we display
 * changes. When user enters new local value, it is displayed no matter what occurs remotely.
 */
template<class T>
struct UserEditableMultiple
{
    bool hasValue() const { return m_user || m_base; }
    T valueOr(T value) const { return m_user.value_or(m_base.value_or(value)); }
    T get() const
    {
        NX_ASSERT(hasValue());
        return valueOr(T{});
    }
    T operator()() const { return get(); }
    operator std::optional<T>() const { return m_user ? m_user : m_base; }

    bool equals(T value) const { return hasValue() && get() == value; }
    bool operator==(T value) const { return equals(value); }
    bool operator==(std::optional<T> value) const { return value ? equals(*value) : !hasValue(); }

    std::optional<T> getBase() const { return m_base; }
    void setBase(T value) { m_base = value; }
    void resetBase() { m_base = {}; }

    bool hasUser() const { return m_user.has_value(); }
    void setUser(T value) { m_user = value; }
    void resetUser() { m_user = {}; }

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
