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

#ifdef Q_OS_IOS
    QnMediaResourceHelper::Protocol nativeStreamProtocol = QnMediaResourceHelper::Hls;
    QnMediaResourceHelper::Protocol transcodingProtocol = QnMediaResourceHelper::Mjpeg;
#else
    QnMediaResourceHelper::Protocol nativeStreamProtocol = QnMediaResourceHelper::Rtsp;
    QnMediaResourceHelper::Protocol transcodingProtocol = QnMediaResourceHelper::Webm;
#endif

    QString protocolName(QnMediaResourceHelper::Protocol protocol) {
        switch (protocol) {
        case QnMediaResourceHelper::Webm:   return lit("webm");
        case QnMediaResourceHelper::Rtsp:   return lit("rtsp");
        case QnMediaResourceHelper::Hls:    return lit("hls");
        case QnMediaResourceHelper::Mjpeg:  return lit("mpjpeg");
        }
        return QString();
    }

    QString protocolScheme(QnMediaResourceHelper::Protocol protocol) {
        if (protocol == QnMediaResourceHelper::Rtsp)
            return lit("rtsp");

        return lit("http");
    }

    QString getAuth(const QnUserResourcePtr &user) {
        QString auth = user->getName() + lit(":0:") + QString::fromLatin1(user->getDigest());
        return QString::fromLatin1(auth.toUtf8().toBase64());
    }

    qint64 convertPosition(qint64 position, QnMediaResourceHelper::Protocol protocol) {
        return protocol == nativeStreamProtocol ? position * 1000 : position;
    }

} // anonymous namespace

QnMediaResourceHelper::QnMediaResourceHelper(QObject *parent) :
    QObject(parent),
    m_position(-1),
    m_nativeStreamIndex(1),
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
        connect(m_resource.data(), &QnResource::parentIdChanged, this, &QnMediaResourceHelper::at_resource_parentIdChanged);
        connect(m_resource.data(), &QnResource::nameChanged,     this, &QnMediaResourceHelper::resourceNameChanged);
        at_resource_parentIdChanged(resource);
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

    if ((m_transcodingSupported && m_resolution.isEmpty() && m_screenSize.isEmpty()) ||
        (!m_transcodingSupported && m_nativeResolutions.isEmpty()))
    {
        setUrl(QUrl());
        return;
    }

    QUrl url(server->getUrl());

    QUrlQuery query;

    Protocol protocol = m_transcodingSupported ? transcodingProtocol : nativeStreamProtocol;
    QnUserResourcePtr user = qnCommon->instance<QnUserWatcher>()->user();

    url.setScheme(protocolScheme(protocol));

    if (protocol == Hls) {
        url.setPath(lit("/hls/%1.m3u").arg(camera->getPhysicalId()));

        query.addQueryItem(m_nativeStreamIndex == 0 ? lit("hi") : lit("lo"), QString());

        if (m_position >= 0)
            query.addQueryItem(lit("startTimestamp"), QString::number(convertPosition(m_position, protocol)));

        url.setUserName(QnAppServerConnectionFactory::url().userName());
        url.setPassword(QnAppServerConnectionFactory::url().password());
    } else {
        if (m_transcodingSupported) {
            url.setPath(lit("/media/%1.%2").arg(camera->getUniqueId()).arg(protocolName(protocol)));
            query.addQueryItem(lit("resolution"), m_resolution.isEmpty() ? optimalResolution() : m_resolution);
            query.addQueryItem(lit("rt"), lit("true"));
        } else {
            url.setPath(lit("/%1").arg(camera->getUniqueId()));
            query.addQueryItem(lit("stream"), QString::number(m_nativeStreamIndex));
        }

        if (m_position >= 0)
            query.addQueryItem(lit("pos"), QString::number(convertPosition(m_position, protocol)));

        if (user)
            query.addQueryItem(lit("auth"), getAuth(user));
    }

    url.setQuery(query);

    setUrl(QnNetworkProxyFactory::instance()->urlToResource(url, server));
}

int QnMediaResourceHelper::nativeStreamIndex(const QString &resolution) const {
    auto it = std::find_if(m_nativeResolutions.begin(), m_nativeResolutions.end(),
                           [&resolution](const QString &res) { return res == resolution; });
    if (it == m_nativeResolutions.end())
        return -1;

    return it.key();
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
    if (!m_transcodingSupported)
        return m_nativeResolutions.values();

    QStringList result;
    for (int resolution: m_standardResolutions)
        result.append(lit("%1p").arg(resolution));
    result.append(QString());
    return result;
}

QString QnMediaResourceHelper::resolution() const {
    if (m_transcodingSupported)
        return m_resolution;
    else
        return m_nativeResolutions.value(m_nativeStreamIndex);
}

void QnMediaResourceHelper::setResolution(const QString &resolution) {
    if (m_transcodingSupported) {
        if (m_resolution == resolution)
            return;

        m_resolution = resolution;
        qnSettings->setLastUsedQuality(m_resolution);
    } else {
        int index = nativeStreamIndex(resolution);

        if (index == -1)
            return;

        if (m_nativeStreamIndex == index)
            return;

        m_nativeStreamIndex = index;
    }

    emit resolutionChanged();

    updateUrl();
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
    if (!m_transcodingSupported)
        return QString();

    if (m_screenSize.isEmpty())
        return QString();

    int maxHeight = qMax(m_screenSize.width(), m_screenSize.height());
    auto it = qUpperBound(m_standardResolutions.begin(), m_standardResolutions.end(), maxHeight);
    int resolution = it == m_standardResolutions.end() ? m_standardResolutions.last() : *it;
    return lit("%1p").arg(resolution);
}

QnMediaResourceHelper::Protocol QnMediaResourceHelper::protocol() const {
    if (m_transcodingSupported)
        return transcodingProtocol;
    else
        return nativeStreamProtocol;
}

void QnMediaResourceHelper::at_resourcePropertyChanged(const QnResourcePtr &resource, const QString &key) {
    Q_ASSERT(m_resource == resource);
    if (m_resource != resource)
        return;

    if (key != Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
        return;

    CameraMediaStreams supportedStreams = QJson::deserialized<CameraMediaStreams>(m_resource->getProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME).toLatin1());

    m_nativeResolutions.clear();
    for (const CameraMediaStreamInfo &info: supportedStreams.streams) {
        if (info.transcodingRequired || info.resolution == CameraMediaStreamInfo::anyResolution)
            continue;

        if (std::find(info.transports.begin(), info.transports.end(), protocolName(nativeStreamProtocol)) != info.transports.end()) {
            if (nativeStreamIndex(info.resolution) != -1)
                continue;
            m_nativeResolutions.insert(info.encoderIndex, info.resolution);
        }
    }
    if (m_nativeResolutions.size() < 2) /* primary and secondary streams */
        m_nativeStreamIndex = m_nativeResolutions.firstKey();

    if (!m_transcodingSupported)
        emit resolutionsChanged();

    updateUrl();
}

void QnMediaResourceHelper::at_resource_parentIdChanged(const QnResourcePtr &resource) {
    if (m_resource != resource)
        return;

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(resource->getParentId());
    bool transcodingSupported = !QnMediaServerResource::isEdgeServer(server);
    if (m_transcodingSupported != transcodingSupported) {
        m_transcodingSupported = transcodingSupported;
        emit resolutionChanged();
        emit resolutionsChanged();
        emit protocolChanged();
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
