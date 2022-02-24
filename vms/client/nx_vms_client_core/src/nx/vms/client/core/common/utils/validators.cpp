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

DoubleValidator::DoubleValidator(QObject* parent): QDoubleValidator(parent)
{
}

void DoubleValidator::registerQmlType()
{
    qmlRegisterType<DoubleValidator>("nx.vms.client.core", 1, 0, "DoubleValidator");
}

double DoubleValidator::lowest()
{
    return std::numeric_limits<double>::lowest();
}

double DoubleValidator::highest()
{
    return std::numeric_limits<double>::max();
}

} // namespace nx::vms::client::core
