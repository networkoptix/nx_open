#include "media_resource_helper.h"

#include <vector>
#include <limits>

#include <QtCore/QUrlQuery>
#include <QtOpenGL/qgl.h>

#include "api/app_server_connection.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/camera_history.h"
#include "common/common_module.h"
#include "network/authutil.h"
#include "http/custom_headers.h"
#include "utils/common/model_functions.h"
#include "utils/common/synctime.h"
#include <nx/network/http/httptypes.h>
#include "watchers/user_watcher.h"
#include <mobile_client/mobile_client_settings.h>
#include <camera/camera_chunk_provider.h>
#include <ui/texture_size_helper.h>

namespace
{

#if defined(Q_OS_IOS)
    const QnMediaResourceHelper::Protocol nativeStreamProtocol = QnMediaResourceHelper::Hls;
    const QnMediaResourceHelper::Protocol transcodingProtocol = QnMediaResourceHelper::Mjpeg;
#elif defined(Q_OS_ANDROID)
    const QnMediaResourceHelper::Protocol nativeStreamProtocol = QnMediaResourceHelper::Rtsp;
    const QnMediaResourceHelper::Protocol transcodingProtocol = QnMediaResourceHelper::Mjpeg;
#else
    const QnMediaResourceHelper::Protocol nativeStreamProtocol = QnMediaResourceHelper::Rtsp;
    const QnMediaResourceHelper::Protocol transcodingProtocol = QnMediaResourceHelper::Mjpeg;
#endif

    enum
    {
        mjpegQuality = 4 // picked as the best balance between traffic and picture quality
    };

    QString protocolName(QnMediaResourceHelper::Protocol protocol)
    {
        switch (protocol)
        {
        case QnMediaResourceHelper::Webm:   return lit("webm");
        case QnMediaResourceHelper::Rtsp:   return lit("rtsp");
        case QnMediaResourceHelper::Hls:    return lit("hls");
        case QnMediaResourceHelper::Mjpeg:  return lit("mpjpeg");
        }
        return QString();
    }

    QString protocolScheme(QnMediaResourceHelper::Protocol protocol)
    {
        if (protocol == QnMediaResourceHelper::Rtsp)
            return lit("rtsp");

        return lit("http");
    }

    QByteArray methodForProtocol(QnMediaResourceHelper::Protocol protocol)
    {
        return protocol == QnMediaResourceHelper::Rtsp ? "PLAY" : "GET";
    }

    QString getAuth(const QnUserResourcePtr &user, QnMediaResourceHelper::Protocol protocol)
    {
        auto nonce = QByteArray::number(qnSyncTime->currentUSecsSinceEpoch(), 16);
        return QString::fromLatin1(createHttpQueryAuthParam(user->getName(), user->getDigest(), methodForProtocol(protocol), nonce));
    }

    qint64 convertPosition(qint64 position, QnMediaResourceHelper::Protocol protocol)
    {
        return protocol == nativeStreamProtocol ? position * 1000 : position;
    }

    QSize sizeFromResolutionString(const QString &resolution)
    {
        int pos = resolution.indexOf(QLatin1Char('x'), 0, Qt::CaseInsensitive);
        if (pos == -1)
            return QSize();

        return QSize(resolution.left(pos).toInt(), resolution.mid(pos + 1).toInt());
    }

    bool transcodingSupportedForServer(const QnMediaServerResourcePtr &server)
    {
        // TODO: #dklychkov Reimplement when we have proper API for this.
        const auto flags = server->getServerFlags();
        return !(flags & (Qn::SF_ArmServer | Qn::SF_Edge));
    }

    QnMediaServerResourcePtr serverAtPosition(
            const QnVirtualCameraResourcePtr &camera,
            qint64 position,
            QnTimePeriod *foundPeriod = nullptr)
    {
        if (position < 0 || position == DATETIME_NOW)
            return camera->getParentServer();

        return qnCameraHistoryPool->getMediaServerOnTime(
                camera, position, foundPeriod);
    }

} // anonymous namespace

QnMediaResourceHelper::QnMediaResourceHelper(QObject *parent)
    : base_type(parent)
    , m_position(-1)
    , m_finalTimestamp(-1)
    , m_resolution(0)
    , m_nativeStreamIndex(1)
    , m_useTranscoding(true)
    , m_transcodingProtocol(transcodingProtocol)
    , m_nativeProtocol(nativeStreamProtocol)
    , m_maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize())
    , m_maxNativeResolution(0)
    , m_chunkProvider(new QnCameraChunkProvider(this))
{
    updateStardardResolutions();
    connect(this, &QnMediaResourceHelper::aspectRatioChanged, this, &QnMediaResourceHelper::rotatedAspectRatioChanged);
    connect(this, &QnMediaResourceHelper::rotationChanged, this, &QnMediaResourceHelper::rotatedAspectRatioChanged);

    connect(qnCameraHistoryPool, &QnCameraHistoryPool::cameraHistoryInvalidated, this,
            [this](const QnVirtualCameraResourcePtr &camera)
            {
                if (!m_camera || m_camera != camera)
                    return;

                updateFinalTimestamp();

                if (m_url.isEmpty())
                    updateUrl();
            }
    );
}

QString QnMediaResourceHelper::resourceId() const
{
    if (!m_camera)
        return QString();
    return m_camera->getId().toString();
}

void QnMediaResourceHelper::setResourceId(const QString &id)
{
    QnVirtualCameraResourcePtr camera = qnResPool->getResourceById<QnVirtualCameraResource>(QnUuid::fromStringSafe(id));
    if (camera == m_camera)
        return;

    if (m_camera)
    {
        disconnect(m_camera, nullptr, this, nullptr);
        m_chunkProvider->setResourceId(QString());
    }

    m_camera = camera;

    if (!m_camera)
    {
        updateUrl();
        return;
    }

    qnCameraHistoryPool->updateCameraHistoryAsync(m_camera, [](bool) {});

    m_nativeProtocol = nativeStreamProtocol;
    m_resolution = qnSettings->lastUsedQuality();

    connect(m_camera, &QnResource::parentIdChanged, this, &QnMediaResourceHelper::at_resource_parentIdChanged);
    connect(m_camera, &QnResource::nameChanged,     this, &QnMediaResourceHelper::resourceNameChanged);
    connect(m_camera, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr &resource, const QString &key)
            {
                NX_ASSERT(m_camera == resource);
                if (m_camera != resource)
                    return;

                if (key == Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
                    updateMediaStreams();
                else if (key == QnMediaResource::rotationKey())
                    emit rotationChanged();

            }
    );
    connect(m_camera, &QnResource::statusChanged,   this, &QnMediaResourceHelper::resourceStatusChanged);

    updateMediaStreams();

    emit resourceIdChanged();
    emit resourceNameChanged();

    emit resolutionChanged();
    emit rotationChanged();
    emit resourceStatusChanged();

    m_chunkProvider->setResourceId(id);
}

Qn::ResourceStatus QnMediaResourceHelper::resourceStatus() const
{
    if (!m_camera)
        return Qn::NotDefined;

    return m_camera->getStatus();
}

QUrl QnMediaResourceHelper::mediaUrl() const
{
    return m_url;
}

void QnMediaResourceHelper::setUrl(const QUrl &url)
{
    if (m_url == url)
        return;

    m_url = url;

    emit mediaUrlChanged();
}

void QnMediaResourceHelper::updateUrl()
{
    if (!m_camera)
    {
        setUrl(QUrl());
        return;
    }

    QnMediaServerResourcePtr server = serverAtCurrentPosition();
    if (!server)
    {
        setUrl(QUrl());
        return;
    }

    if ((m_useTranscoding && m_resolution <= 0 && m_screenSize.isEmpty()) ||
        (!m_useTranscoding && m_nativeResolutions.isEmpty()))
    {
        setUrl(QUrl());
        return;
    }


    QUrl url(qnCommon->currentServer()->getUrl());

    QUrlQuery query;

    Protocol protocol = m_useTranscoding ? m_transcodingProtocol : m_nativeProtocol;
    QnUserResourcePtr user = qnCommon->instance<QnUserWatcher>()->user();

    url.setScheme(protocolScheme(protocol));

    if (protocol == Hls)
    {
        url.setPath(lit("/hls/%1.m3u").arg(m_camera->getPhysicalId()));

        query.addQueryItem(m_nativeStreamIndex == 0 ? lit("hi") : lit("lo"), QString());

        if (m_position >= 0)
            query.addQueryItem(lit("startTimestamp"), QString::number(convertPosition(m_position, protocol)));
    }
    else
    {
        if (m_useTranscoding)
        {
            url.setPath(lit("/media/%1.%2").arg(m_camera->getUniqueId()).arg(protocolName(protocol)));
            query.addQueryItem(lit("resolution"), currentResolutionString());
            query.addQueryItem(lit("rt"), lit("true"));
            query.addQueryItem(lit("rotation"), lit("0"));
        }
        else if (protocol == Mjpeg)
        {
            url.setPath(lit("/media/%1.%2").arg(m_camera->getUniqueId()).arg(protocolName(protocol)));
            query.addQueryItem(lit("resolution"), m_nativeResolutions[m_nativeStreamIndex]);
        }
        else
        {
            url.setPath(lit("/%1").arg(m_camera->getUniqueId()));
            query.addQueryItem(lit("stream"), QString::number(m_nativeStreamIndex));
        }

        if (m_position >= 0)
            query.addQueryItem(lit("pos"), QString::number(convertPosition(m_position, protocol)));
    }

    if (user)
        query.addQueryItem(lit("auth"), getAuth(user, protocol));

    if (protocol == Mjpeg)
    {
        query.addQueryItem(lit("ct"), lit("false"));
        query.addQueryItem(lit("quality"), QString::number(mjpegQuality));
    }

    query.addQueryItem(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME), qnCommon->runningInstanceGUID().toString());
    query.addQueryItem(QLatin1String(Qn::CUSTOM_USERNAME_HEADER_NAME), QnAppServerConnectionFactory::url().userName());
    query.addQueryItem(QLatin1String(Qn::USER_AGENT_HEADER_NAME), QLatin1String(nx_http::userAgentString()));

    url.setQuery(query);

    setUrl(url);
}

int QnMediaResourceHelper::nativeStreamIndex(const QString &resolution) const
{
    auto it = std::find_if(m_nativeResolutions.begin(), m_nativeResolutions.end(),
                           [&resolution](const QString &res) { return res == resolution; });
    if (it == m_nativeResolutions.end())
        return -1;

    return it.key();
}

QString QnMediaResourceHelper::resolutionString(int resolution) const
{
    if (resolution <= 0)
        return QString();

    return lit("%1x%2").arg(int(sensorAspectRatio() * resolution)).arg(resolution);
}

QString QnMediaResourceHelper::currentResolutionString() const
{
    return resolutionString(m_resolution > 0 ? m_resolution : optimalResolution());
}

QString QnMediaResourceHelper::resourceName() const
{
    if (!m_camera)
        return QString();

    return m_camera->getName();
}

QnCameraChunkProvider *QnMediaResourceHelper::chunkProvider() const
{
    return m_chunkProvider;
}

qint64 QnMediaResourceHelper::position() const
{
    return m_position;
}

void QnMediaResourceHelper::setPosition(qint64 position)
{
    if (m_position == position)
        return;

    m_position = position;

    emit positionChanged();
    updateFinalTimestamp();
    updateCurrentStream();
}

QStringList QnMediaResourceHelper::resolutions() const
{
    if (!m_useTranscoding)
        return m_nativeResolutions.values();

    QStringList result;
    for (int resolution: m_standardResolutions)
        result.append(lit("%1p").arg(resolution));
    result.append(QString());
    return result;
}

QString QnMediaResourceHelper::resolution() const
{
    if (m_useTranscoding)
    {
        return m_resolution > 0 ? lit("%1p").arg(m_resolution) : QString();
    }
    else
    {
        return m_nativeResolutions.value(m_nativeStreamIndex);
    }
}

void QnMediaResourceHelper::setResolution(const QString &resolution)
{
    if (m_useTranscoding)
    {
        int resolutionHeight = resolution.leftRef(resolution.length() - 1).toInt();

        if (m_resolution == resolutionHeight)
            return;

        m_resolution = resolutionHeight;
        qnSettings->setLastUsedQuality(m_resolution);
    }
    else
    {
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

QSize QnMediaResourceHelper::screenSize() const
{
    return m_screenSize;
}

void QnMediaResourceHelper::setScreenSize(const QSize &size)
{
    if (m_screenSize == size)
        return;

    QString resolution = currentResolutionString();

    m_screenSize = size;

    bool updateRequired = resolution != currentResolutionString();

    emit screenSizeChanged();

    if (updateRequired)
        updateUrl();
}

int QnMediaResourceHelper::optimalResolution() const
{
    if (!m_useTranscoding)
        return 0;

    if (m_screenSize.isEmpty())
        return 0;

    int maxHeight = qMax(m_screenSize.width(), m_screenSize.height());
    auto it = std::upper_bound(m_standardResolutions.begin(), m_standardResolutions.end(), maxHeight);
    return it == m_standardResolutions.end() ? m_standardResolutions.last() : *it;
}

int QnMediaResourceHelper::maximumResolution() const
{
    int maxResolution = std::numeric_limits<int>::max();

    qreal ar = aspectRatio();
    if (!qFuzzyIsNull(ar))
    {
        if (ar > 1)
            maxResolution = int(m_maxTextureSize / ar);
        else
            maxResolution = int(m_maxTextureSize * ar);
    }

    if (m_maxNativeResolution > 0)
        maxResolution = qMin(maxResolution, m_maxNativeResolution * m_camera->getVideoLayout()->size().height());

    return maxResolution;
}

void QnMediaResourceHelper::updateFinalTimestamp()
{
    if (!m_camera)
    {
        setFinalTimestamp(-1);
        return;
    }

    if (m_position <= 0)
    {
        setFinalTimestamp(-1);
        return;
    }

    if (!qnCameraHistoryPool->isCameraHistoryValid(m_camera))
    {
        qnCameraHistoryPool->updateCameraHistoryAsync(m_camera, [this](bool success)
        {
            if (success)
                updateFinalTimestamp();
            else
                setFinalTimestamp(-1);
        });
        return;
    }

    QnTimePeriod period;
    serverAtPosition(m_camera, m_position, &period);

    setFinalTimestamp(period.isValid() && !period.isInfinite() ? period.endTimeMs() : -1);
}

void QnMediaResourceHelper::setFinalTimestamp(qint64 finalTimestamp)
{
    if (m_finalTimestamp == finalTimestamp)
        return;

    m_finalTimestamp = finalTimestamp;
    emit finalTimestampChanged();
}

QnMediaServerResourcePtr QnMediaResourceHelper::serverAtCurrentPosition() const
{
    qint64 position = m_position;
    if (position != -1)
        position = m_chunkProvider->closestChunkStartMs(position, true);

    return serverAtPosition(m_camera, position);
}

QnMediaResourceHelper::Protocol QnMediaResourceHelper::protocol() const
{
    if (m_useTranscoding)
        return m_transcodingProtocol;
    else
        return m_nativeProtocol;
}

qreal QnMediaResourceHelper::aspectRatio() const
{
    qreal aspectRatio = sensorAspectRatio();
    if (qFuzzyIsNull(aspectRatio))
        return 0;

    QSize layoutSize = m_camera->getVideoLayout()->size();
    if (!layoutSize.isEmpty())
        aspectRatio *= qreal(layoutSize.width()) / layoutSize.height();

    return aspectRatio;
}

qreal QnMediaResourceHelper::sensorAspectRatio() const
{
    if (!m_camera)
        return 0;

    qreal aspectRatio = m_camera->customAspectRatio();

    if (qFuzzyIsNull(aspectRatio) && !m_nativeResolutions.isEmpty())
    {
        QString resolution;

        if (m_nativeResolutions.size() == 1)
        {
            resolution = m_nativeResolutions.first();
        }
        else
        {
            if (m_useTranscoding)
                resolution = m_nativeResolutions.first();
            else
                resolution = m_nativeResolutions[m_nativeStreamIndex];
        }

        QSize size = sizeFromResolutionString(resolution);
        if (!size.isEmpty())
            aspectRatio = qreal(size.width()) / size.height();
    }

    return aspectRatio;
}

qreal QnMediaResourceHelper::rotatedAspectRatio() const
{
    qreal ar = aspectRatio();
    if (qFuzzyIsNull(ar))
        return ar;

    int rot = rotation();
    if (rot % 90 == 0 && rot % 180 != 0)
        return 1.0 / ar;

    return ar;
}

int QnMediaResourceHelper::rotation() const
{
    if (!m_camera)
        return 0;

    return m_camera->getProperty(QnMediaResource::rotationKey()).toInt();
}

qint64 QnMediaResourceHelper::finalTimestamp() const
{
    return m_finalTimestamp;
}

void QnMediaResourceHelper::updateMediaStreams()
{
    if (!m_camera)
        return;

    CameraMediaStreams supportedStreams = QJson::deserialized<CameraMediaStreams>(m_camera->getProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME).toLatin1());

    m_nativeResolutions.clear();
    bool nativeSupported = false;
    bool mjpegSupported = false;

    m_maxNativeResolution = 0;

    for (const CameraMediaStreamInfo &info: supportedStreams.streams)
    {
        if (info.transcodingRequired || info.resolution == CameraMediaStreamInfo::anyResolution)
            continue;

        bool hasNative = std::find(info.transports.begin(), info.transports.end(), protocolName(nativeStreamProtocol)) != info.transports.end();
        bool hasMjpeg = std::find(info.transports.begin(), info.transports.end(), lit("mjpeg")) != info.transports.end();

        nativeSupported |= hasNative;
        mjpegSupported |= hasMjpeg;

        m_maxNativeResolution = qMax(m_maxNativeResolution, sizeFromResolutionString(info.resolution).height());

        if (hasNative || hasMjpeg)
        {
            if (nativeStreamIndex(info.resolution) != -1)
                continue;

            m_nativeResolutions.insert(info.encoderIndex, info.resolution);
        }
    }

    if (!m_nativeResolutions.isEmpty())
    {
        if (m_nativeResolutions.size() < 2)
        {
            /* primary and secondary streams */
            m_nativeStreamIndex = m_nativeResolutions.firstKey();
        }
        else
        {
            int maxResolution = maximumResolution();
            for (auto it = m_nativeResolutions.begin(); it != m_nativeResolutions.end(); /* no inc */)
            {
                QSize size = sizeFromResolutionString(it.value());
                if (size.height() > maxResolution)
                    it = m_nativeResolutions.erase(it);
                else
                    ++it;
            }
        }
    }

    Protocol nativeProtocol = m_nativeProtocol;

    if (mjpegSupported && !nativeSupported)
        nativeProtocol = Mjpeg;
    else
        nativeProtocol = nativeStreamProtocol;

    bool needUpdateUrl = m_url.isEmpty();

    if (m_nativeProtocol != nativeProtocol)
    {
        m_nativeProtocol = nativeProtocol;
        if (!m_useTranscoding)
            needUpdateUrl = true;
    }

    if (needUpdateUrl)
        updateCurrentStream();
}

void QnMediaResourceHelper::updateCurrentStream()
{
    if (!m_camera)
        return;

    QnTimePeriod period;
    QnMediaServerResourcePtr server = serverAtPosition(
            m_camera, m_position, &period);
    if (!server)
        return;

    bool useTranscoding = transcodingSupportedForServer(server);

    if (m_useTranscoding != useTranscoding)
    {
        m_useTranscoding = useTranscoding;

        if (m_useTranscoding)
            updateStardardResolutions();

        emit resolutionsChanged();
        emit resolutionChanged();
        emit protocolChanged();
        emit aspectRatioChanged();
    }
    else if (m_useTranscoding && m_url.isEmpty())
    {
        updateStardardResolutions();
        emit resolutionsChanged();
    }

    updateUrl();
}

void QnMediaResourceHelper::at_resource_parentIdChanged(const QnResourcePtr &resource)
{
    if (m_camera != resource)
        return;

    updateCurrentStream();
}

void QnMediaResourceHelper::updateStardardResolutions()
{
    m_standardResolutions.clear();

    int maxResolution = maximumResolution();

    for (int resolution: { 240, 360, 480, 720, 1080 })
    {
        if (resolution <= maxResolution)
            m_standardResolutions.append(resolution);
    }

    if (m_standardResolutions.isEmpty())
        m_standardResolutions.append(maxResolution);

    maxResolution = m_standardResolutions.last();

    if (m_resolution > 0 && m_resolution > maxResolution)
        m_resolution = maxResolution;
}
