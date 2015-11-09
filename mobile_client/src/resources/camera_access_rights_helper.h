#pragma once

class QnCameraAccessRughtsHelperPrivate;
class QnCameraAccessRughtsHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool canViewArchive READ canViewArchive NOTIFY canViewArchiveChanged)

public:
    explicit QnCameraAccessRughtsHelper(QObject *parent = nullptr);
    ~QnCameraAccessRughtsHelper();

    QString resourceId() const;
    void setResourceId(const QString &id);

    bool canViewArchive() const;

signals:
    void resourceIdChanged();
    void canViewArchiveChanged();

private:
    QScopedPointer<QnCameraAccessRughtsHelperPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnCameraAccessRughtsHelper)
};
