// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "validators.h"

#include <limits>

#include <QtQml/QtQml>

namespace nx::vms::client::core {

IntValidator::IntValidator(QObject* parent): QIntValidator(parent)
{
}

void IntValidator::registerQmlType()
{
    qmlRegisterType<IntValidator>("nx.vms.client.core", 1, 0, "IntValidator");
}

int IntValidator::lowest()
{
    return std::numeric_limits<int>::lowest();
}

int IntValidator::highest()
{
    return std::numeric_limits<int>::max();
}

DoubleValidator::DoubleValidator(QObject* parent):
    QDoubleValidator(lowest(), highest(), /*decimals*/ -1, parent)
{
}

void DoubleValidator::registerQmlType()
{
    qmlRegisterType<DoubleValidator>("nx.vms.client.core", 1, 0, "DoubleValidator");
}

double DoubleValidator::lowest()
{
    // QDoubleValidator works correctly only when the range values are within the int limits,
    // because it casts double to int during validation.
    return std::numeric_limits<int>::min() + 1;
}

double DoubleValidator::highest()
{
    // QDoubleValidator works correctly only when the range values are within the int limits,
    // because it casts double to int during validation.
    return std::numeric_limits<int>::max();
}

} // namespace nx::vms::client::core
