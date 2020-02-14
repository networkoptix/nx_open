#include "validators.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

IntValidator::IntValidator(QObject* parent): QIntValidator(parent)
{
}

void IntValidator::registerQmlType()
{
    qmlRegisterType<IntValidator>("nx.vms.client.core", 1, 0, "IntValidator");
}

DoubleValidator::DoubleValidator(QObject* parent): QDoubleValidator(parent)
{
}

void DoubleValidator::registerQmlType()
{
    qmlRegisterType<DoubleValidator>("nx.vms.client.core", 1, 0, "DoubleValidator");
}

} // namespace nx::vms::client::core
