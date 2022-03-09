// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
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
    /**
     * Base stored value.
     */
    T base = T{};

    /**
     * User-defined value, overriding base value.
     */
    std::optional<T> user;

    T get() const { return user.value_or(base); }
    T operator()() const { return get(); }

     T getBase() const { return base; }
    void setBase(T value) { base = value; }

    bool hasUser() const { return user.has_value(); }
    void setUser(T value) { user = value; }
    void resetUser() { user = std::nullopt; }
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(UserEditable, (base)(user))

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
    /**
     * Base stored value. Filled if it is the same for all sources.
     */
    std::optional<T> base;

    /**
     * User-defined value, overriding base value.
     */
    std::optional<T> user;

    bool hasValue() const { return user || base; }
    T valueOr(T value) const { return user.value_or(base.value_or(value)); }
    T get() const
    {
        NX_ASSERT(hasValue());
        return valueOr(T{});
    }
    T operator()() const { return get(); }
    operator std::optional<T>() const { return user ? user : base; }

    bool equals(T value) const { return hasValue() && get() == value; }
    bool operator==(T value) const { return equals(value); }
    bool operator==(std::optional<T> value) const { return value ? equals(*value) : !hasValue(); }

    std::optional<T> getBase() const { return base; }
    void setBase(T value) { base = value; }
    void resetBase() { base = {}; }

    bool hasUser() const { return user.has_value(); }
    void setUser(T value) { user = value; }
    void resetUser() { user = {}; }

    static UserEditableMultiple<T> fromValue(const T& value)
    {
        UserEditableMultiple<T> result;
        result.setBase(value);
        return result;
    }
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(UserEditableMultiple, (base)(user))

NX_REFLECTION_ENUM_CLASS(CombinedValue,
    None,
    Some,
    All
);

} // namespace nx::vms::client::desktop
