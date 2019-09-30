#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QnUuid;

namespace nx::client::mobile {

class AudioController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool audioEnabled READ audioEnabled WRITE setAudioEnabled NOTIFY audioEnabledChanged)

public:
    static void registerQmlType();

    AudioController(QObject* parent = nullptr);
    virtual ~AudioController();

    void setSessionParameters(const QnUuid& localSystemId, const QString& user);

    void setResourceId(const QnUuid& value);
    QnUuid resourceId() const;

    Q_INVOKABLE void setAudioEnabled(bool value);
    bool audioEnabled() const;

signals:
    void resourceIdChanged();
    void audioEnabledChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::client::mobile
