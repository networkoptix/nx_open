// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional.h"

#include <nx/fusion/model_functions.h>

namespace {
    const QString undefined(lit("__qn_undefined_value__"));
}


QnOptionalBool::QnOptionalBool()
{}

QnOptionalBool::QnOptionalBool( bool value )
    : m_value(value)
{}

bool QnOptionalBool::value() const
{
    return *m_value;
}

void QnOptionalBool::setValue( bool value ) {
    m_value = value;
}

bool QnOptionalBool::isDefined() const {
    return static_cast<bool>(m_value);
}

void QnOptionalBool::undefine() {
    m_value.reset();
}

bool QnOptionalBool::operator==( const QnOptionalBool &other ) const {
    /* If defined, check if other is defined and its value */
    if (m_value)
        return other.m_value && (*m_value == *other.m_value);

    /* Otherwise check if it is undefined too. */
    return !other.m_value;
}

void serialize(const QnOptionalBool &value, QString *target) {
    if (value.isDefined())
        QnLexical::serialize(value.value(), target);
    else
        QnLexical::serialize(undefined, target);
}

bool deserialize(const QString &value, QnOptionalBool *target) {
    if (value == undefined) {
        target->undefine();
        return true;
    }

    bool result = false;
    if (!QnLexical::deserialize(value, &result))
        return false;
    target->setValue(result);
    return true;
}
