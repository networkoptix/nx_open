#include "entropix_image_enhancer.h"

#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QHttpMultiPart>

#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>
#include <nx/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <ui/common/geometry.h>

#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace  {

QByteArray imageToByteArray(const QImage& image)
{
    QByteArray data;
    QBuffer buffer(&data);
    image.save(&buffer, "png");
    return data;
}

} // namespace

class EntropixImageEnhancer::Private: public QObject
{
    EntropixImageEnhancer* q = nullptr;

    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_replyData;

public:
    Private(EntropixImageEnhancer* parent);

    QnVirtualCameraResourcePtr camera;
    qint64 timestamp;
    QRectF zoomRect;
    QScopedPointer<CameraThumbnailProvider> thumbnailLoader;

    enum class Step
    {
        idle = -1,
        gettingScreenshot,
        waitingForImageProcessed,
        downloadingImage,

        stepsCount
    };
    int progress = -1;

    void cancel();

    void setProgress(Step step, int stepProgress = 0);
    void setProgress(int progress);

    void enhanceScreenshot(const QImage& colorImage, const QImage& blackAndWhiteImage);

    void at_imageLoaded(const QImage& image);

    void at_replyReadyRead();
    void at_replyFinished();
};

EntropixImageEnhancer::Private::Private(EntropixImageEnhancer* parent):
    q(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void EntropixImageEnhancer::Private::cancel()
{
    if (thumbnailLoader)
        thumbnailLoader.reset();

    if (m_reply)
        delete m_reply;

    NX_DEBUG(q, "Request cancelled");

    setProgress(Step::idle);
}

void EntropixImageEnhancer::Private::setProgress(
    EntropixImageEnhancer::Private::Step step, int stepProgress)
{
    if (step == Step::idle)
    {
        setProgress(-1);
        return;
    }

    constexpr int count = static_cast<int>(Step::stepsCount);
    constexpr int stepSize = 100 / count;

    const int intStep = static_cast<int>(step);

    const int newProgress = intStep * 100 / count + stepSize * stepProgress / 100;

    if (newProgress != progress)
    {
        progress = newProgress;
        emit q->progressChanged(progress);
    }
}

void EntropixImageEnhancer::Private::setProgress(int progress)
{
    if (progress == this->progress)
        return;

    this->progress = progress;
    emit q->progressChanged(progress);
}

void EntropixImageEnhancer::Private::enhanceScreenshot(
    const QImage& colorImage, const QImage& blackAndWhiteImage)
{
    m_replyData.clear();
    setProgress(Step::waitingForImageProcessed);

    const auto& colorImageData = imageToByteArray(colorImage);
    const auto& blackAndWhiteImageData = imageToByteArray(blackAndWhiteImage);
    const auto& rect = QnGeometry::subRect(colorImage.rect(), zoomRect).toRect();

    auto makePart =
        [](const QByteArray& name, const QByteArray& body)
        {
            QHttpPart part;
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                QByteArray("form-data; name=\"") + name + "\"");
            part.setBody(body);
            return part;
        };

    auto makeImagePart =
        [](const QByteArray& name, const QByteArray& fileName, const QByteArray& body)
        {
            QHttpPart part;
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                QByteArray("form-data; name=\"") + name
                    + "\"; filename=\"" + fileName + "\"");
            part.setHeader(
                QNetworkRequest::ContentTypeHeader,
                QByteArray("application/octet-stream"));
            part.setBody(body);
            return part;
        };

    auto multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multiPart->append(makePart("x", QByteArray::number(rect.x())));
    multiPart->append(makePart("y", QByteArray::number(rect.y())));
    multiPart->append(makePart("width", QByteArray::number(rect.width())));
    multiPart->append(makePart("height", QByteArray::number(rect.height())));
    multiPart->append(makeImagePart("cimg", "color.png", colorImageData));
    multiPart->append(makeImagePart("pimg", "bw.png", blackAndWhiteImageData));

    const QUrl url(QLatin1String(ini().entropixEnhancerUrl));

    NX_DEBUG(q,
        lm("Requesting enhanced image of size %1x%2 from %3")
            .arg(colorImage.width())
            .arg(colorImage.height())
            .arg(url));
    m_reply = m_networkAccessManager->post(QNetworkRequest(url), multiPart);
    multiPart->setParent(m_reply);

    connect(m_reply.data(), &QNetworkReply::readyRead, this, &Private::at_replyReadyRead);
    connect(m_reply.data(), &QNetworkReply::finished, this, &Private::at_replyFinished);
}

void EntropixImageEnhancer::Private::at_imageLoaded(const QImage& image)
{
    NX_DEBUG(q, lm("Got screenshot with size %1x%2").arg(image.width()).arg(image.height()));

    const auto& sensors = camera->combinedSensorsDescription();

    const auto& colorSensor = sensors.colorSensor();
    const auto& blackAndWhiteSensor = sensors.blackAndWhiteSensor();

    if (!colorSensor.isValid() || !blackAndWhiteSensor.isValid())
    {
        emit q->cameraScreenshotReady(image);
        NX_ERROR(q, "Cannot extract separate sensors from the screenshot.");
        return;
    }

    const auto& colorImage = image.copy(colorSensor.frameSubRect(image.size()));
    const auto& blackAndWhiteImage = image.copy(blackAndWhiteSensor.frameSubRect(image.size()));

    enhanceScreenshot(colorImage, blackAndWhiteImage);
}

void EntropixImageEnhancer::Private::at_replyReadyRead()
{
    if (sender() != m_reply)
        return;

    m_replyData.append(m_reply->readAll());
    NX_VERBOSE(q, lm("Reading data: %1").arg(m_replyData.size()));

    const auto size = m_reply->size();
    setProgress(Step::downloadingImage, size > 0 ? m_replyData.size() * 100 / size : 0);
}

void EntropixImageEnhancer::Private::at_replyFinished()
{
    if (sender() != m_reply)
        return;

    m_reply->deleteLater();

    if (m_reply->error() == QNetworkReply::NoError)
    {
        NX_DEBUG(q, "Request finished");
    }
    else
    {
        NX_ERROR(q, lm("Request failed: %1").arg(m_reply->errorString()));
        setProgress(Step::idle);
        return;
    }

    m_replyData.append(m_reply->readAll());

    const auto& image = QImage::fromData(m_replyData);

    NX_DEBUG(q, lm("Got image of size %1x%2").arg(image.width()).arg(image.height()));

    setProgress(Step::idle);

    emit q->cameraScreenshotReady(image);
}

EntropixImageEnhancer::EntropixImageEnhancer(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this))
{
    d->camera = camera;
}

EntropixImageEnhancer::~EntropixImageEnhancer()
{
}

void EntropixImageEnhancer::requestScreenshot(qint64 timestamp, const QRectF& zoomRect)
{
    d->setProgress(Private::Step::gettingScreenshot);

    d->zoomRect = zoomRect;

    if (!d->thumbnailLoader)
    {
        api::CameraImageRequest request;
        request.camera = d->camera;
        request.msecSinceEpoch = timestamp;
        d->thumbnailLoader.reset(new CameraThumbnailProvider(request));
    }

    connect(d->thumbnailLoader.data(), &CameraThumbnailProvider::imageChanged,
        d.data(), &Private::at_imageLoaded);

    NX_DEBUG(this,
        lm("Requesting screenshot from server for camera %2 at %1")
            .arg(timestamp).arg(d->camera->getName()));
    d->thumbnailLoader->loadAsync();
}

void EntropixImageEnhancer::cancelRequest()
{
    d->cancel();
}

} // namespace desktop
} // namespace client
} // namespace nx
