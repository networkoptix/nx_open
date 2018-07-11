#ifdef ENABLE_ONVIF

#include "newdw_ptz_controller.h"
#include "plugins/resource/digitalwatchdog/digital_watchdog_resource.h"

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

using namespace nx::core;

static const int CACHE_UPDATE_TIMEOUT = 60 * 1000;

QnNewDWPtzController::QnNewDWPtzController(const QnDigitalWatchdogResourcePtr &resource):
    QnBasicPtzController(resource),
    m_resource(resource)
{
    QnPtzPresetList tmp;
    getPresets(&tmp);
}

QnNewDWPtzController::~QnNewDWPtzController()
{

}

Ptz::Capabilities QnNewDWPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return Ptz::ContinuousZoomCapability | Ptz::ContinuousPanCapability
        | Ptz::PresetsPtzCapability | Ptz::NativePresetsPtzCapability;
}

static QString zoomDirection(qreal speed)
{
    if(qFuzzyIsNull(speed))
        return lit("stop");
    else if(speed < 0)
        return lit("out");
    else
        return lit("in");
}

static int toNativeSpeed(qreal speed)
{
    return int (qAbs(speed * 10) + 0.5);
}

static QString panDirection(qreal speed)
{
    if(qFuzzyIsNull(speed))
        return lit("stop");
    else if(speed < 0)
        return lit("left");
    else
        return lit("right");
}

bool QnNewDWPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    QString request;
    if (!qFuzzyIsNull(speedVector.zoom))
    {
        request = lm("/cgi-bin/ptz.cgi?zoom=%1").arg(zoomDirection(speedVector.zoom));
    }
    else
    {
        request = lm("/cgi-bin/ptz.cgi?speed=%1&move=%2")
            .args(toNativeSpeed(speedVector.pan), panDirection(speedVector.pan));
    }
    return doQuery(request);
}

bool QnNewDWPtzController::doQuery(const QString &request, QByteArray* body) const
{
    static const int TCP_TIMEOUT = 1000 * 5;
    QString resourceUrl = m_resource->getUrl();
    QUrl url;
    if (resourceUrl.contains(lit("http")))
        url = QUrl(resourceUrl);
    else {
        url.setScheme(lit("http"));
        url.setHost(resourceUrl);
        url.setPort(80);
    }
    int queryPos = request.indexOf(L'?');
    if (queryPos == -1) {
        url.setPath(request);
    }
    else {
        url.setPath(request.left(queryPos));
        url.setQuery(request.mid(queryPos+1));
    }
    QString encodedPath = url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters | QUrl::RemoveScheme | QUrl::RemoveAuthority);
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, m_resource->getAuth());
    CLHttpStatus status = client.doGET(encodedPath);
    if (status == CL_HTTP_SUCCESS && body)
        client.readAll(*body);
    return status == CL_HTTP_SUCCESS;
}

bool QnNewDWPtzController::getPresets(QnPtzPresetList *presets) const
{
    if (m_cacheUpdateTimer.isNull() || m_cacheUpdateTimer.elapsed() > CACHE_UPDATE_TIMEOUT)
    {
        QByteArray body;
        bool rez = doQuery(lit("/cgi-bin/ptz.cgi?list=preset"), &body);
        if (!rez)
            return rez;
        QnMutexLocker lock( &m_mutex );
        for(QByteArray line: body.split(L'\n'))
        {
            line = line.trimmed();
            if (line.startsWith("list="))
            {
                QList<QByteArray> presetTokens = line.split(L'=')[1].split(L' ');
                for(const QByteArray& token: presetTokens)
                {
                    if (token.isEmpty())
                        continue;
                    m_cachedData.insert(QString::fromUtf8(token), QString(tr("Preset #")) + QString::fromUtf8(token));
                }
            }
        }
    }

    for (auto itr = m_cachedData.begin(); itr != m_cachedData.end(); ++itr)
        presets->push_back(QnPtzPreset(itr.key(), itr.value()));
    return true;
}

QString QnNewDWPtzController::toInternalID(const QString& externalId)
{
    QnMutexLocker lock( &m_mutex );

    int num = 1;
    static const int MAX_PRESETS = 16;
    for (; num < MAX_PRESETS; ++num)
    {
        if (!m_cachedData.contains(QString::number(num)))
            break;
    }
    QString result = QString::number(num);
    m_extIdToIntId.insert(externalId, result);
    return result;
}

QString QnNewDWPtzController::fromExtarnalID(const QString& externalId)
{
    QnMutexLocker lock( &m_mutex );
    QString result = m_extIdToIntId.value(externalId);
    return result.isEmpty() ? externalId : result;
}

bool QnNewDWPtzController::activatePreset(const QString &presetId, qreal /*speed*/)
{
    return doQuery(lit("/cgi-bin/ptz.cgi?gotopreset=%1").arg(fromExtarnalID(presetId)));
}

bool QnNewDWPtzController::createPreset(const QnPtzPreset &preset)
{
    QString internalId = toInternalID(preset.id);
    bool rez = doQuery(lit("/cgi-bin/ptz.cgi?savepreset=%1&label=%2").arg(internalId).arg(preset.name));
    if (rez) {
        m_cachedData[internalId] = QString(tr("Preset #")) + internalId;
        m_cacheUpdateTimer.restart();
    }
    return rez;
}

bool QnNewDWPtzController::updatePreset(const QnPtzPreset &preset)
{
    return createPreset(preset);
}

bool QnNewDWPtzController::removePreset(const QString &presetId)
{
    QString internalId = fromExtarnalID(presetId);
    bool rez = doQuery(lit("/cgi-bin/ptz.cgi?removepreset=%1").arg(internalId));
    if (rez) {
        m_cachedData.remove(internalId);
        m_cacheUpdateTimer.restart();
    }
    return rez;
}

#endif // ENABLE_ONVIF
