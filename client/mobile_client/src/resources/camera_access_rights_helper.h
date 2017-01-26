#pragma once

class QnCameraAccessRightsHelperPrivate;
class QnCameraAccessRightsHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool canViewArchive READ canViewArchive NOTIFY canViewArchiveChanged)

public:
    explicit QnCameraAccessRightsHelper(QObject *parent = nullptr);
    ~QnCameraAccessRightsHelper();

    QString resourceId() const;
    void setResourceId(const QString &id);

    bool canViewArchive() const;

signals:
    void resourceIdChanged();
    void canViewArchiveChanged();

private:
    QScopedPointer<QnCameraAccessRightsHelperPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnCameraAccessRightsHelper)
};
