#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace nx::vms::client::core {

class OperationManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    OperationManager(QObject* parent = nullptr);

    static void registerQmlType();

    using CallbackFunction = std::function<void (bool success)>;
    QString startOperation(const CallbackFunction& callback);
    Q_INVOKABLE void finishOperation(const QString& id, bool success);

private:
    QHash<QString, CallbackFunction> m_operations;
};

} // namespace nx::vms::client::core
