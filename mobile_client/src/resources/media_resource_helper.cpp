#include "media_resource_helper.h"

#include <vector>
#include <limits>

#include <QtCore/QUrlQuery>

#include "api/app_server_connection.h"
#include "api/network_proxy_factory.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/camera_history.h"
#include "common/common_module.h"
#include "utils/common/model_functions.h"
#include "watchers/user_watcher.h"
#include <mobile_client/mobile_client_settings.h>

namespace {

    QnMediaResourceHelper::Protocol pickBestProtocol(const std::vector<QString> &transports) {
        QnMediaResourceHelper::Protocol protocol = QnMediaResourceHelper::UnknownProtocol;

        if (std::find(transports.begin(), transports.end(), lit("webm")) != transports.end())
            protocol = QnMediaResourceHelper::Http;
        else if (std::find(transports.begin(), transports.end(), lit("rtsp")) != transports.end())
            protocol = QnMediaResourceHelper::Rtsp;

        return protocol;
    }

    QSize resolutionFromString(const QString &resolutionString, const QSize &defaultSize = QSize()) {
        int xIndex = resolutionString.indexOf(QLatin1Char('x'));
        if (xIndex == -1) {
            if (resolutionString == CameraMediaStreamInfo::anyResolution)
                return defaultSize;
            return QSize();
        }

        bool ok;
        int width, height;
        width = resolutionString.left(xIndex).toInt(&ok);
        if (ok)
            height = resolutionString.mid(xIndex + 1).toInt(&ok);
        if (ok)
            return QSize(width, height);
        return QSize();
    }

    QString getAuth(const QnUserResourcePtr &user) {
        QString auth = user->getName() + lit(":0:") + QString::fromLatin1(user->getDigest());
        return QString::fromLatin1(auth.toUtf8().toBase64());
    }

    qint64 convertPosition(qint64 position, QnMediaResourceHelper::Protocol protocol) {
        return protocol == QnMediaResourceHelper::Rtsp ? position * 1000 : position;
    }

} // anonymous namespace

QnMediaResourceHelper::QnMediaResourceHelper(QObject *parent) :
    QObject(parent),
    m_position(-1),
    m_protocol(Http),
    m_nativeStreamIndex(-1),
    m_transcodingSupported(true)
{
    setStardardResolutions();
}

QString QnMediaResourceHelper::resourceId() const {
    if (!m_resource)
        return QString();
    return m_resource->getId().toString();
}

void QnMediaResourceHelper::setResourceId(const QString &id) {
    QnUuid uuid(id);

    QnResourcePtr resource = qnResPool->getResourceById(uuid);
    if (resource == m_resource)
        return;

    if (m_resource)
        disconnect(m_resource.data(), nullptr, this, nullptr);

    m_resource = resource;

    if (resource) {
        m_resource = resource;

        connect(m_resource.data(), &QnResource::propertyChanged, this, &QnMediaResourceHelper::at_resourcePropertyChanged);
        at_resourcePropertyChanged(resource, Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME);

        emit resourceIdChanged();
        emit resourceNameChanged();

        m_resolution = qnSettings->lastUsedQuality();
        emit resolutionChanged();
    }

    updateUrl();
}

QUrl QnMediaResourceHelper::mediaUrl() const {
    return m_url;
}

void QnMediaResourceHelper::setUrl(const QUrl &url) {
    if (m_url == url)
        return;

    m_url = url;

    emit mediaUrlChanged();
}

void QnMediaResourceHelper::updateUrl() {
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera) {
        setUrl(QUrl());
        return;
    }

    QnTimePeriod period;
    QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, m_position, &period);
    if (!server) {
        setUrl(QUrl());
        return;
    }

    if (m_nativeStreamIndex < 0 && (m_resolution.isEmpty() && m_screenSize.isEmpty())) {
        setUrl(QUrl());
        return;
    }

    QUrl url(server->getUrl());

    QUrlQuery query;

    switch (m_protocol) {
    case Rtsp:
        url.setScheme(lit("rtsp"));
        url.setPath(lit("/%1").arg(camera->getMAC().toString()));
        if (m_nativeStreamIndex < 0)
            query.addQueryItem(lit("codec"), lit("h263p"));
        break;
    case Http: /* default */
    default:
        url.setScheme(lit("http"));
        url.setPath(lit("/media/%1.webm").arg(camera->getMAC().toString()));
    }

    if (m_nativeStreamIndex >= 0) {
        // TODO: #dklychkov implement
        // query.addQueryItem(lit("stream"), m_nativeStreamIndex);
    } else {
        query.addQueryItem(lit("resolution"), m_resolution.isEmpty() ? optimalResolution() : m_resolution);
    }

    if (m_position >= 0)
        query.addQueryItem(lit("pos"), QString::number(convertPosition(m_position, m_protocol)));

    if (QnUserResourcePtr user = qnCommon->instance<QnUserWatcher>()->user())
        query.addQueryItem(lit("auth"), getAuth(user));

    url.setQuery(query);

    setUrl(QnNetworkProxyFactory::instance()->urlToResource(url, server));
}


QString QnMediaResourceHelper::resourceName() const {
    if (!m_resource)
        return QString();

    return m_resource->getName();
}

void QnMediaResourceHelper::setPosition(qint64 position) {
    if (m_position == position)
        return;

    m_position = position;

    updateUrl();
}

QStringList QnMediaResourceHelper::resolutions() const {
    QStringList result;

    if (m_transcodingSupported) {
        for (int resolution: m_standardResolutions)
            result.append(lit("%1p").arg(resolution));
        result.append(QString());
    } else {
        // TODO: #dklychkov implement
    }

    return result;
}

QString QnMediaResourceHelper::resolution() const {
    if (m_nativeStreamIndex >= 0) {
        // TODO: #dklychkov
    }
    return m_resolution;
}

void QnMediaResourceHelper::setResolution(const QString &resolution) {
    if (m_resolution == resolution)
        return;

    m_resolution = resolution;

    emit resolutionChanged();

    updateUrl();

    if (m_transcodingSupported)
        qnSettings->setLastUsedQuality(m_resolution);
}

QSize QnMediaResourceHelper::screenSize() const {
    return m_screenSize;
}

void QnMediaResourceHelper::setScreenSize(const QSize &size) {
    if (m_screenSize == size)
        return;

    m_screenSize = size;

    emit screenSizeChanged();

    updateUrl();
}

QString QnMediaResourceHelper::optimalResolution() const {
    if (m_nativeStreamIndex >= 0)
        return QString();

    if (m_screenSize.isEmpty())
        return QString();

    int maxHeight = qMax(m_screenSize.width(), m_screenSize.height());
    auto it = qUpperBound(m_standardResolutions.begin(), m_standardResolutions.end(), maxHeight);
    int resolution = it == m_standardResolutions.end() ? m_standardResolutions.last() : *it;
    return lit("%1p").arg(resolution);
}

void QnMediaResourceHelper::at_resourcePropertyChanged(const QnResourcePtr &resource, const QString &key) {
    Q_ASSERT(m_resource == resource);
    if (m_resource != resource)
        return;

    if (key != Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
        return;

    m_supportedStreams = QJson::deserialized<CameraMediaStreams>(m_resource->getProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME).toLatin1());

    m_protocol = UnknownProtocol;
    m_aspectRatio = -1;
    for (const CameraMediaStreamInfo &info: m_supportedStreams.streams) {
        Protocol protocol = pickBestProtocol(info.transports);
        if (protocol < m_protocol)
            m_protocol = protocol;

        if (m_aspectRatio < 0 && info.resolution != CameraMediaStreamInfo::anyResolution) {
            QSize size = resolutionFromString(info.resolution);
            if (!size.isNull())
                m_aspectRatio = static_cast<qreal>(size.width()) / size.height();
        }
    }

    updateUrl();
}

void QnMediaResourceHelper::setStardardResolutions() {
    m_standardResolutions.append(240);
    m_standardResolutions.append(360);
    m_standardResolutions.append(480);
    m_standardResolutions.append(720);
    m_standardResolutions.append(1080);
}
