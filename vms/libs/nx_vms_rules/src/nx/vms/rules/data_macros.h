// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

// Do not include this header to base class definitions to avoid global macro propagation.

namespace nx::vms::rules {

/** Macro for simple data field definition. */
#define FIELD(type, getter, setter) \
    Q_PROPERTY(type getter READ getter WRITE setter) \
public: \
    type getter() const { return m_##getter; } \
    void setter(const type &val) { m_##getter = val; } \
private: \
    type m_##getter;

} // namespace nx::vms::rules
