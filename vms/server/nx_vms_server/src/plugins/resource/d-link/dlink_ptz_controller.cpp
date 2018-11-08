#ifdef ENABLE_ONVIF

#include "dlink_ptz_controller.h"
#include "plugins/resource/onvif/onvif_resource.h"

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

using namespace nx::core;
//static const int CACHE_UPDATE_TIMEOUT = 60 * 1000;

class QnDlinkPtzRepeatCommand : public QnLongRunnable
{
public:
    QnDlinkPtzRepeatCommand(QnDlinkPtzController* controller): m_controller(controller)
    {

    }

    virtual void run() override
    {
        static const int SLEEP_TIME = 10;
        int delay = 250;
        for (int repCnt = 0; repCnt < 20 && !m_needStop; ++repCnt)
        {
            for (int i = 0; i < delay / SLEEP_TIME; ++i) {
                msleep(SLEEP_TIME);
                if (m_needStop)
                    return;
            }
            if (!m_controller->doQuery(m_controller->getLastCommand()))
                return;
            delay = 100;
        }
    }

    QnDlinkPtzController* m_controller;
};

QnDlinkPtzController::QnDlinkPtzController(const QnPlOnvifResourcePtr &resource):
    QnBasicPtzController(resource),
    m_resource(resource)
{
    m_repeatCommand.reset(new QnDlinkPtzRepeatCommand(this));
}

QnDlinkPtzController::~QnDlinkPtzController()
{

}

Ptz::Capabilities QnDlinkPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return Ptz::ContinuousPtzCapabilities;
}

static QString zoomDirection(qreal speed)
{
    if(speed > 0)
        return lit("tele");
    else
        return lit("wide");
}

static QString moveDirection(qreal x, qreal y)
{
    if(qAbs(x) > qAbs(y))
        return x > 0 ? lit("right") : lit("left");
    else
        return y > 0 ? lit("up") : lit("down");
}

bool QnDlinkPtzController::continuousMove(
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
        request = lm("/cgi-bin/camctrl/camctrl.cgi?channel=%1&zoom=%2")
            .args(m_resource->getChannel()).arg(zoomDirection(speedVector.zoom));
    }
    else if (!qFuzzyIsNull(speedVector.pan) || !qFuzzyIsNull(speedVector.tilt))
    {
        request = lm("/cgi-bin/camctrl/camctrl.cgi?channel=%1&move=%2")
            .args(m_resource->getChannel()).arg(moveDirection(speedVector.pan, speedVector.tilt));
    }

    m_repeatCommand->stop();
    bool rez = true;
    if (!request.isEmpty())
        rez = doQuery(request);

    if (rez) {
        QnMutexLocker lock( &m_mutex );
        m_lastRequest = request;
        if (!m_lastRequest.isEmpty())
            m_repeatCommand->start();
    }

    return rez;
}

bool QnDlinkPtzController::doQuery(const QString &request, QByteArray* body) const
{
    if (request.isEmpty())
        return false;

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

QString QnDlinkPtzController::getLastCommand() const
{
    QnMutexLocker lock( &m_mutex );
    return m_lastRequest;
}

#endif // ENABLE_ONVIF
