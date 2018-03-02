#pragma once

#include <QtCore/QObject>

#include <api/server_rest_connection_fwd.h>

namespace nx {
namespace client {
namespace mobile {

class SoftwareTriggersController: public QObject
{
    Q_OBJECT
    using base_object = QObject;

public:
    SoftwareTriggersController(QObject* parent = nullptr);

private:
    Q_INVOKABLE bool activateTrigger(const QString& resourceId, const QnUuid& id);
    Q_INVOKABLE bool cancelTriggerAction(const QnUuid& id);

private:
    using IdToHandleHash = QHash<QnUuid, rest::Handle>;
    using HandleToIdHash = QHash<rest::Handle, QnUuid>;

    IdToHandleHash m_idToHandle;
    HandleToIdHash m_handleToId;
};

} // namespace mobile
} // namespace client
} // namespace nx
