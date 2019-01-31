
#ifdef ENABLE_ARECONT

#include "arecont_meta_reader.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

QnMetaDataV1Ptr ArecontMetaReader::parseMotionMetadata(
    int channel, const QString& response, const ParsingInfo& info)
{
    int index = response.indexOf('=');
    if (index == -1)
        return nullptr;

    QString motionStr = response.mid(index + 1);
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    motion->channelNumber = channel;
    if (motionStr == lit("no motion"))
        return motion; // no motion detected

    int zoneCount = info.totalMdZones == 1024 ? 32 : 8;
    QStringList md = motionStr.split(L' ', QString::SkipEmptyParts);
    if (md.size() < zoneCount * zoneCount)
        return nullptr;

    int pixelZoneSize = info.zoneSize * 32;
    if (pixelZoneSize == 0)
        return nullptr;

    QRect imageRect(0, 0, info.maxSensorWidth, info.maxSensorHight);
    QRect zeroZoneRect(0, 0, pixelZoneSize, pixelZoneSize);

    for (int x = 0; x < zoneCount; ++x)
    {
        for (int y = 0; y < zoneCount; ++y)
        {
            int index = y * zoneCount + x;
            QString m = md.at(index);
            if (m == lit("00") || m == lit("0"))
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);
            motion->mapMotion(imageRect, currZoneRect);
        }
    }
    // TODO #lbusygin change to some reasonable value
    motion->m_duration = 1000 * 1000 * 1000; // 1000 sec
    return motion;
}

ArecontMetaReader::ArecontMetaReader(
        int channelCount, std::chrono::milliseconds minRepeatInterval, int minFrameInterval):
    m_channelCount(channelCount),
    m_minRepeatInterval(minRepeatInterval),
    m_minFrameInterval(minFrameInterval)
{
}

ArecontMetaReader::~ArecontMetaReader()
{
    m_metaDataClient.pleaseStopSync();
}

bool ArecontMetaReader::hasData()
{
    QnMutexLocker lock(&m_metaDataMutex);
    return m_metaData != nullptr;
}

QnMetaDataV1Ptr ArecontMetaReader::getData()
{
    QnMutexLocker lock(&m_metaDataMutex);
    return std::move(m_metaData);
}

void ArecontMetaReader::requestIfReady(QnPlAreconVisionResource* resource)
{
    bool repeatIntervalExceeded = m_framesSinceLastMetaData > 0 &&
        (!m_lastMetaRequest.isValid() || m_lastMetaRequest.elapsed() > m_minRepeatInterval);
    bool frameIntervalExceeded = m_framesSinceLastMetaData > m_minFrameInterval;

    if (!m_metaDataClientBusy && (repeatIntervalExceeded || frameIntervalExceeded))
    {
        m_framesSinceLastMetaData = 0;
        m_lastMetaRequest.restart();
        requestAsync(resource);
    }
}

void ArecontMetaReader::requestAsync(QnPlAreconVisionResource* resource)
{
    QString path = "/get";
    if (m_channelCount > 1)
        path += QString::number(m_currentChannel + 1);
    auto auth = resource->getAuth();
    auto url = nx::network::url::Builder()
        .setScheme("http")
        .setHost(resource->getHostAddress())
        .setPort(80)
        .setPath(path)
        .setQuery("mdresult")
        .setUserName(auth.user())
        .setPassword(auth.password());

    ParsingInfo info {
        resource->totalMdZones(),
        resource->getZoneSite(),
        resource->getProperty(lit("MaxSensorWidth")).toInt(),
        resource->getProperty(lit("MaxSensorHeight")).toInt()
    };
    m_metaDataClientBusy = true;
    m_metaDataClient.doGet(url,
        [this, info](nx::network::http::AsyncHttpClientPtr)
        {
            onMetaData(info);
            if (m_channelCount > 1)
                m_currentChannel = (m_currentChannel + 1) % m_channelCount;
            m_metaDataClientBusy = false;
        }
    );
}

void ArecontMetaReader::onMetaData(const ParsingInfo& info)
{
    if (m_metaDataClient.state() == nx::network::http::AsyncClient::State::sFailed)
    {
        NX_WARNING(this, lm("Failed to get motion meta data, error: %1").arg(
            SystemError::toString(m_metaDataClient.lastSysErrorCode())));
        return;
    }

    if (!m_metaDataClient.response())
    {
        NX_WARNING(this, lm("Failed to get motion meta data, invalid response"));
        return;
    }

    const int statusCode = m_metaDataClient.response()->statusLine.statusCode;
    if (statusCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(this, lm("Failed to get motion meta data. Http error code %1")
            .arg(statusCode));
        return;
    }
    QString result = QString::fromLatin1(m_metaDataClient.fetchMessageBodyBuffer());
    QnMutexLocker lock(&m_metaDataMutex);
    m_metaData = parseMotionMetadata(m_currentChannel, result, info);
    if (!m_metaData)
    {
        NX_WARNING(this,
            lm("Failed to get motion meta data. Invalid response format %1").arg(result));
    }
}

#endif // ENABLE_ARECONT
