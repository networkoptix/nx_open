#ifdef ENABLE_ARECONT

#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>

#include "../dataprovider/panoramic_cpul_tftp_dataprovider.h"
#include "core/resource/resource_media_layout.h"

#include "av_resource.h"
#include "av_panoramic.h"
#include "core/resource_management/resource_properties.h"

#define MAX_RESPONSE_LEN (4*1024)


QnArecontPanoramicResource::QnArecontPanoramicResource(const QString& name)
{
    setName(name);
    m_isRotated = false;
    m_flipTimer.invalidate();
}

QnArecontPanoramicResource::~QnArecontPanoramicResource()
{
}


bool QnArecontPanoramicResource::getDescription()
{
    m_description = QString();
    return true;
}

QnAbstractStreamDataProvider* QnArecontPanoramicResource::createLiveDataProvider()
{
    NX_LOG( lit("Create live provider for camera %1").arg(getHostAddress()), cl_logDEBUG1);
    return new AVPanoramicClientPullSSTFTPStreamreader(toSharedPointer());
}

bool QnArecontPanoramicResource::getParamPhysical2(int channel, const QString& name, QVariant &val)
{
    m_mutex.lock();
    m_mutex.unlock();
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());
    QString request = QLatin1String("get") + QString::number(channel) + QLatin1String("?") + name;

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED)
        setStatus(Qn::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;


    QByteArray response;
    connection.readAll(response);
    int index = response.indexOf('=');
    if (index==-1)
        return false;

    val = QLatin1String(response.mid(index+1));

    return true;
}

bool QnArecontPanoramicResource::setParamPhysical(const QString &param, const QVariant& val )
{
    if (setSpecialParam(param, val))
        return true;
    QUrl devUrl(getUrl());
    for (int i = 1; i <=4 ; ++i)
    {
        CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

        QString request = QLatin1String("set") + QString::number(i) + QLatin1Char('?') + param + QLatin1Char('=') + val.toString();

        if (connection.doGET(request)!=CL_HTTP_SUCCESS)
            if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
                return false;
    }
    return true;
}

bool QnArecontPanoramicResource::setSpecialParam(const QString& name, const QVariant& val)
{
    if (name == QLatin1String("resolution"))
    {
        if (val.toString() == QLatin1String("half"))
            return setResolution(false);
        if (val.toString() == QLatin1String("full"))
            return setResolution(true);
    }
    else if (name == QLatin1String("Quality"))
    {
        int q = val.toInt();
        if (q >= 1 && q <= 21)
            return setCamQuality(q);
    }

    return false;
}

CameraDiagnostics::Result QnArecontPanoramicResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    const CameraDiagnostics::Result result = QnPlAreconVisionResource::initInternal();
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    setRegister(3, 100, 10); // sets I frame frequency to 10

    setParamPhysical(QLatin1String("cnannelenable"), 15); // to enable all channels

    getVideoLayout(0);

    return CameraDiagnostics::NoErrorResult();
}

void QnArecontPanoramicResource::updateFlipState()
{
    if (m_flipTimer.isValid() && m_flipTimer.elapsed() < 1000)
        return;
    m_flipTimer.restart();
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());
    QString request = QLatin1String("get?rotate");
    CLHttpStatus responseCode = connection.doGET(request);
    if (responseCode != CL_HTTP_SUCCESS)
        return;

    QByteArray response;
    connection.readAll(response);
    int valPos = response.indexOf('=');
    if (valPos >= 0) {
        m_isRotated = response.mid(valPos+1).trimmed().toInt();
    }
}

//=======================================================================
bool QnArecontPanoramicResource::setResolution(bool full)
{
    int value = full ? 15 : 0; // all sensors to full/half resolution

    if (CL_HTTP_SUCCESS!=setRegister(3, 0xD1, value)) // FULL RES QULITY
        return false;

    return true;
}

bool QnArecontPanoramicResource::setCamQuality(int q)
{
    if (CL_HTTP_SUCCESS!=setRegister(3, 0xED, q)) // FULL RES QULITY
        return false;

    if (CL_HTTP_SUCCESS!=setRegister(3, 0xEE, q)) // HALF RES QULITY
        return false;

    return false;

}

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getDefaultVideoLayout() const
{
    if (m_defaultVideoLayout)
        return m_defaultVideoLayout;

    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId()); 
    if (resType)
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(QnCustomResourceVideoLayout::fromString(resType->defaultValue(Qn::VIDEO_LAYOUT_PARAM_NAME)));
    else
        m_defaultVideoLayout = QnResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout());
    return m_defaultVideoLayout;
}

QnConstResourceVideoLayoutPtr QnArecontPanoramicResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const auto resourceId = getId();    //saving id before locking m_layoutMutex to avoid potential deadlock

    Q_UNUSED(dataProvider)
    QMutexLocker lock(&m_layoutMutex);

    QnConstResourceVideoLayoutPtr layout = QnArecontPanoramicResource::getDefaultVideoLayout();
    const QnCustomResourceVideoLayout* customLayout = dynamic_cast<const QnCustomResourceVideoLayout*>(layout.data());
    if (!customLayout)
        return layout;
    auto nonConstThis = const_cast<QnArecontPanoramicResource*>(this);
    nonConstThis->updateFlipState();
    if (m_isRotated)
    {
        if (!m_rotatedLayout) {
            m_rotatedLayout.reset( new QnCustomResourceVideoLayout(customLayout->size()) );
            QVector<int> channels = customLayout->getChannels();
            std::reverse(channels.begin(), channels.end());
            m_rotatedLayout->setChannels(channels);
        }
        layout = m_rotatedLayout;
    }

    QString oldVideoLayout = propertyDictionary->value(resourceId, Qn::VIDEO_LAYOUT_PARAM_NAME); // get from kvpairs directly. do not read default value from resourceTypes
    QString newVideoLayout = layout->toString();
    if (newVideoLayout != oldVideoLayout) {
        propertyDictionary->setValue(resourceId, Qn::VIDEO_LAYOUT_PARAM_NAME, newVideoLayout);
        propertyDictionary->saveParams(resourceId );
    }

    return layout;
}

#endif
