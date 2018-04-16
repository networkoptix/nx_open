#include "mobile_client_ui_controller.h"

namespace {

QString uuidString(const QnUuid& uuid)
{
    return uuid.isNull() ? QString() : uuid.toString();
}

} // anonymous namespace

class QnMobileClientUiControllerPrivate
{
public:
    QString layoutId;
};

QnMobileClientUiController::QnMobileClientUiController(QObject* parent) :
    QObject(parent),
    d_ptr(new QnMobileClientUiControllerPrivate())
{
}

QnMobileClientUiController::~QnMobileClientUiController()
{
}

QString QnMobileClientUiController::layoutId() const
{
    Q_D(const QnMobileClientUiController);
    return d->layoutId;
}

void QnMobileClientUiController::setLayoutId(const QString& layoutId)
{
    Q_D(QnMobileClientUiController);
    if (d->layoutId == layoutId)
        return;

    d->layoutId = layoutId;
    emit layoutIdChanged();
}

void QnMobileClientUiController::disconnectFromSystem()
{
    emit disconnectRequested();
    setLayoutId(QString());
}

void QnMobileClientUiController::connectToSystem(
    const nx::utils::Url& url)
{
    emit connectRequested(url);
}

void QnMobileClientUiController::openConnectToServerScreen(
    const nx::utils::Url& url,
    const QString& operationId)
{
    const auto port = url.port(-1);
    const auto host = lit("%1%2").arg(url.host(),
        port > 0 ? lit(":1").arg(QString::number(port)) : QString());
    emit connectToServerScreenRequested(host, url.userName(), url.password(), operationId);
}

void QnMobileClientUiController::openResourcesScreen(const ResourceIdList& filterIds)
{
    emit resourcesScreenRequested(QVariant::fromValue(filterIds));
}

void QnMobileClientUiController::openVideoScreen(const QnUuid& cameraId, qint64 timestamp)
{
    emit videoScreenRequested(uuidString(cameraId), timestamp);
}

void QnMobileClientUiController::openLoginToCloudScreen(
    const QString& user,
    const QString& password,
    const QUuid& connectOperationId)
{
    emit loginToCloudScreenRequested(user, password, connectOperationId.toString());
}
