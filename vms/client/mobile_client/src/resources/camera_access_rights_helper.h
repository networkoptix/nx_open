#pragma once

#include <client_core/connection_context_aware.h>

class QnCameraAccessRightsHelperPrivate;
class QnCameraAccessRightsHelper: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool canViewArchive READ canViewArchive NOTIFY canViewArchiveChanged)
    Q_PROPERTY(bool canManagePtz READ canManagePtz NOTIFY canManagePtzChanged)
public:
    explicit QnCameraAccessRightsHelper(QObject *parent = nullptr);
    ~QnCameraAccessRightsHelper();

    QString resourceId() const;
    void setResourceId(const QString &id);

    bool canViewArchive() const;

    bool canManagePtz() const;

signals:
    void resourceIdChanged();
    void canViewArchiveChanged();
    void canManagePtzChanged();

private:
    QScopedPointer<QnCameraAccessRightsHelperPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnCameraAccessRightsHelper)
};
