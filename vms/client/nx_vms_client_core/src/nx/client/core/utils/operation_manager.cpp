#include "operation_manager.h"

#include <QtCore/QUuid>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

OperationManager::OperationManager(QObject* parent):
    base_type(parent)
{
}

void OperationManager::registerQmlType()
{
    qmlRegisterType<OperationManager>("nx.client.core", 1, 0, "OperationManager");
}

QString OperationManager::startOperation(const CallbackFunction& callback)
{
    const auto id = QUuid::createUuid().toString();
    m_operations.insert(id, callback);
    return id;
}

void OperationManager::finishOperation(const QString& id, bool success)
{
    const auto it = m_operations.find(id);
    if (it == m_operations.end())
    {
        NX_ASSERT(false, "Can't find async operation");
        return;
    }

    const auto callback = it.value();
    m_operations.erase(it);
    if (callback)
        callback(success);
}

} // namespace nx::vms::client::core

