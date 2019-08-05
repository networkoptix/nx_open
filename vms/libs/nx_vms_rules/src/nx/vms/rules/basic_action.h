#pragma once

#include <nx/vms/api/rules/event_info.h>

#include <chrono>

namespace nx::vms::rules {

enum class ActionType
{
    Instant = 0,
    ActAtStart = 1 << 0,
    ActAtEnd = 1 << 1,
    Prolonged = ActAtStart | ActAtEnd
};

class NX_VMS_RULES_API BasicAction: public QObject
{
    Q_OBJECT

public:
    QString type() const;

protected:
    BasicAction(const QString &type);

private:
    QString m_type;
};

using ActionPtr = QSharedPointer<BasicAction>;

class NX_VMS_RULES_API NotificationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT

    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString description READ description WRITE setDescription)

public:
    NotificationAction(): BasicAction("nx.showNotification"){}

    QString caption() const;
    void setCaption(const QString& caption);

    QString description() const;
    void setDescription(const QString& caption);

private:
    QString m_caption;
    QString m_description;
};

} // namespace nx::vms::rules