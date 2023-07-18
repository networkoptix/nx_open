// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::core {

#define DEFINE_STANDARD_PROPERTY(type, name /*also getter*/, setter, defaultValue) \
    Q_PROPERTY(type name READ name WRITE setter NOTIFY name##Changed) \
public: \
    type name() const { return m_##name; } \
    void setter(const type& value) \
    { \
        if (m_##name == value) \
            return; \
        m_##name = value; \
        emit name##Changed(); \
    } \
    Q_SIGNAL void name##Changed(); \
private: \
    type m_##name{defaultValue};

/**
 * A singleton that holds transient properties that persist only through the current client run.
 */
class GlobalTemporaries: public QObject
{
    Q_OBJECT

    DEFINE_STANDARD_PROPERTY(bool, automaticAccessDependencySwitchVisible,
        setAutomaticAccessDependencySwitchVisible, false)
    DEFINE_STANDARD_PROPERTY(bool, automaticAccessDependenciesEnabledByDefault,
        setAutomaticAccessDependenciesEnabledByDefault, true)

public:
    static GlobalTemporaries* instance();
    static void registerQmlType();
};

#undef DEFINE_PROPERTY

} // namespace nx::vms::client::core
