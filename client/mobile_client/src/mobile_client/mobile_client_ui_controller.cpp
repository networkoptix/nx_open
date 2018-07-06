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

void QnMobileClientUiController::connectToSystem(const nx::utils::Url& url)
{
    emit connectRequested(url);
}

void QnMobileClientUiController::openResourcesScreen()
{
    emit resourcesScreenRequested();
}

void QnMobileClientUiController::openVideoScreen(const QnUuid& cameraId)
{
    emit videoScreenRequested(uuidString(cameraId));
}
