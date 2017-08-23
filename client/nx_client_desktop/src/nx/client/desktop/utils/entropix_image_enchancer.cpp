#include "entropix_image_enchancer.h"

#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtNetwork/QHttpMultiPart>

#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>
#include <camera/single_thumbnail_loader.h>
#include <ui/common/geometry.h>

namespace nx {
namespace client {
namespace desktop {

namespace  {

static const QUrl kEntropixUrl(lit("http://96.64.226.250:8888/image"));

QByteArray imageToByteArray(const QImage& image)
{
    QByteArray data;
    QBuffer buffer(&data);
    image.save(&buffer, "png");
    return data;
}

} // namespace

class EntropixImageEnchancer::Private: public QObject
{
    EntropixImageEnchancer* q = nullptr;

    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_replyData;

public:
    Private(EntropixImageEnchancer* parent);

    QnVirtualCameraResourcePtr camera;
    qint64 timestamp;
    QRectF zoomRect;
    QScopedPointer<QnSingleThumbnailLoader> thumbnailLoader;

    void cancel();

    void enchanceScreenchot(const QImage& colorImage, const QImage& blackAndWhiteImage);

    void at_imageLoaded(const QByteArray& imageData);

    void at_replyReadyRead();
    void at_replyFinished();
};

EntropixImageEnchancer::Private::Private(EntropixImageEnchancer* parent):
    q(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void EntropixImageEnchancer::Private::cancel()
{
    NX_DEBUG(q, "Request cancelled");
    if (thumbnailLoader)
        thumbnailLoader.reset();

    if (m_reply)
        delete m_reply;
}

void EntropixImageEnchancer::Private::enchanceScreenchot(
    const QImage& colorImage, const QImage& blackAndWhiteImage)
{
    m_replyData.clear();

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
        [&makePart](const QByteArray& name, const QByteArray& fileName, const QByteArray& body)
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

    NX_DEBUG(q,
        lm("Requesting enchanced image of size %1x%2 from %3")
            .arg(colorImage.width())
            .arg(colorImage.height())
            .arg(kEntropixUrl.toString()));
    m_reply = m_networkAccessManager->post(QNetworkRequest(kEntropixUrl), multiPart);
    multiPart->setParent(m_reply);

    connect(m_reply, &QNetworkReply::readyRead, this, &Private::at_replyReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &Private::at_replyFinished);
}

void EntropixImageEnchancer::Private::at_imageLoaded(const QByteArray& imageData)
{
    const auto& image = QImage::fromData(imageData);
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

    enchanceScreenchot(colorImage, blackAndWhiteImage);
}

void EntropixImageEnchancer::Private::at_replyReadyRead()
{
    if (sender() != m_reply)
        return;

    m_replyData.append(m_reply->readAll());
    NX_VERBOSE(q, lm("Reading data: %1").arg(m_replyData.size()));
}

void EntropixImageEnchancer::Private::at_replyFinished()
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
        return;
    }

    m_replyData.append(m_reply->readAll());

    const auto& image = QImage::fromData(m_replyData);

    NX_DEBUG(q, lm("Got image of size %1x%2").arg(image.width()).arg(image.height()));

    emit q->cameraScreenshotReady(image);
}

EntropixImageEnchancer::EntropixImageEnchancer(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this))
{
    d->camera = camera;
}

EntropixImageEnchancer::~EntropixImageEnchancer()
{
}

void EntropixImageEnchancer::requestScreenshot(qint64 timestamp, const QRectF& zoomRect)
{
    d->zoomRect = zoomRect;

    if (!d->thumbnailLoader)
        d->thumbnailLoader.reset(new QnSingleThumbnailLoader(d->camera, timestamp));

    connect(d->thumbnailLoader.data(), &QnSingleThumbnailLoader::imageLoaded,
        d.data(), &Private::at_imageLoaded);

    NX_DEBUG(this,
        lm("Requesting screenshot from server for camera %2 at %1")
            .arg(timestamp).arg(d->camera->getName()));
    d->thumbnailLoader->loadAsync();
}

void EntropixImageEnchancer::cancelRequest()
{
    d->cancel();
}

} // namespace desktop
} // namespace client
} // namespace nx
