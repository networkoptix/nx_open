#ifndef MEDIA_RESOURCE_HELPER_H
#define MEDIA_RESOURCE_HELPER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnMediaResourceHelper : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QUrl mediaUrl READ mediaUrl NOTIFY mediaUrlChanged)
public:
    explicit QnMediaResourceHelper(QObject *parent = 0);

    QString resourceId() const;
    void setResourceId(const QString &id);

    QUrl mediaUrl() const;

signals:
    void resourceIdChanged();
    void mediaUrlChanged();

private:
    QnResourcePtr m_resource;

};

#endif // MEDIA_RESOURCE_HELPER_H
