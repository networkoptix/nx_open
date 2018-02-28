#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace mobile {

struct SoftwareTriggerData
{
    QString triggerId;
    QString name;
    bool prolonged = false;
};

// TODO: add check if resource is camera
class SoftwareTriggersWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    SoftwareTriggersWatcher(QObject* parent = nullptr);

    void setResourceId(const QnUuid& resourceId);

    void forcedUpdateEnabledStates();

    bool isEnabled(const QnUuid& id) const;

    QString icon(const QnUuid& id) const;

signals:
    void resourceIdChanged();

    void triggerAdded(
        const QnUuid& id,
        const SoftwareTriggerData& data,
        const QString& iconPath,
        bool enabled);

    void triggerRemoved(const QnUuid& id);

    void enabledChanged(const QnUuid& id);
    void iconChanged(const QnUuid& id);

private:
    void clearTriggers();
    void updateTriggers();
    void removeTrigger(const QnUuid& id);

private:
    struct Description;
    using DescriptionPtr = QSharedPointer<Description>;
    using DescriptionsHash = QHash<QnUuid, DescriptionPtr>;

    QnUuid m_resourceId;
    DescriptionsHash m_data;
};

} // namespace mobile
} // namespace client
} // namespace nx
