#pragma once

#include <QtCore/QObject>

class QnUuid;

namespace nx::client::mobile {

class EnabledAudioController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool audioEnabled READ audioEnabled WRITE setAudioEnabled NOTIFY audioEnabledChanged)

public:
    static void registerQmlType();

    EnabledAudioController(QObject* parent = nullptr);
    virtual ~EnabledAudioController();

    void setSessionParameters(const QnUuid& localSystemId, const QString& user);

    void setResourceId(const QString& value);
    QString resourceId() const;

    Q_INVOKABLE void setAudioEnabled(bool value);
    bool audioEnabled() const;

signals:
    void resourceIdChanged();
    void audioEnabledChanged();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // nx::client::mobile
