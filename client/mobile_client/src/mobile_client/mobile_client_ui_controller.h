#pragma once

#include <QtCore/QObject>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)

    using ResourceIdList = QList<QnUuid>;

public:
    QnMobileClientUiController(QObject* parent = nullptr);
    ~QnMobileClientUiController();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

public slots:
    void disconnectFromSystem();
    void connectToSystem(const nx::utils::Url &url);

    void openConnectToServerScreen(const nx::utils::Url& url, const QString& operationId);
    void openResourcesScreen(const ResourceIdList& filterIds = ResourceIdList());
    void openVideoScreen(const QnUuid& cameraId, qint64 timestamp);
    void openLoginToCloudScreen(
        const QString& user,
        const QString& password,
        const QUuid& connectOperationId);

signals:
    void connectRequested(const nx::utils::Url& url);
    void disconnectRequested();
    void layoutIdChanged();
    void resourcesScreenRequested(const QVariant& filterIds);
    void videoScreenRequested(const QString& cameraId, qint64 timestamp);

    void loginToCloudScreenRequested(
        const QString& user,
        const QString& password,
        const QString& connectOperationId);

    void connectToServerScreenRequested(
        const QString& host,
        const QString& user,
        const QString& password,
        const QString& operationId);

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
