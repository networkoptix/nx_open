#pragma once

#include <QtCore/QObject>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/vms/utils/system_uri.h>

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutIdChanged)

public:
    QnMobileClientUiController(QObject* parent = nullptr);
    ~QnMobileClientUiController();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    using ResourceIdList = nx::vms::utils::SystemUri::ResourceIdList;

public slots:
    void disconnectFromSystem();
    void connectToSystem(const nx::utils::Url &url);

    void openResources(const ResourceIdList& resourceIds = ResourceIdList());
    void openResourcesScreen();
    void openVideoScreen(const QnUuid& cameraId);
    void openLoginToCloudScreen(
        const QString& user,
        const QString& password,
        const QUuid& connectOperationId);

signals:
    void connectRequested(const nx::utils::Url& url);
    void disconnectRequested();
    void layoutIdChanged();
    void resourcesScreenRequested();
    void videoScreenRequested(const QString& cameraId);

    void loginToCloudScreenRequested(
        const QString& user,
        const QString& password,
        const QString& connectOperationId);

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
