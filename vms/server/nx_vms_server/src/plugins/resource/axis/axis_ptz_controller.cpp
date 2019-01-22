#ifdef ENABLE_AXIS

#include "axis_ptz_controller.h"

#include <common/common_module.h>

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/fusion/serialization/lexical_functions.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_data.h>

#include "axis_resource.h"

using namespace nx::core;

static const int DEFAULT_AXIS_API_PORT = 80; // TODO: #Elric copypasta from axis_resource.cpp
static const int CACHE_UPDATE_TIMEOUT = 60 * 1000;

namespace {

static const int kDefaultPanRange = 360;
static const int kDefaultTiltRange = 180;
static const int kDefaultZoomRange = 9999;
static const int kDefaultFocusRange = 9999;

int range(double minValue, double maxValue, int defaultValue)
{
    if (qFuzzyEquals(minValue, maxValue) || maxValue < minValue)
        return defaultValue;

    return maxValue - minValue;
}

ptz::Vector toAxisRelativeMovement(const ptz::Vector& movementVector, const QnPtzLimits& limits)
{
    ptz::Vector result;
    result.pan = movementVector.pan * range(limits.minPan, limits.maxPan, kDefaultPanRange);
    result.tilt = movementVector.tilt * range(limits.minTilt, limits.maxTilt, kDefaultTiltRange);
    result.zoom = movementVector.zoom * range(limits.minFov, limits.maxFov, kDefaultZoomRange);
    result.focus = movementVector.focus * range(limits.minFocus, limits.maxFocus, kDefaultFocusRange);

    return result;
}

} // namespace

// TODO: #Elric #EC2 use QnIniSection

// -------------------------------------------------------------------------- //
// QnAxisParameterMap
// -------------------------------------------------------------------------- //
class QnAxisParameterMap {
public:
    QnAxisParameterMap() {};

    void insert(const QString& key, const QString& value)
    {
        m_data[key] = value;
    }

    template<class T>
    T value(const QString& key, const T& defaultValue = T()) const
    {
        QString result = m_data.value(key);
        if(result.isNull())
            return defaultValue;

        T value;
        if(QnLexical::deserialize(result, &value))
            return value;
        else
            return defaultValue;
    }

    template<class T>
    bool value(const QString& key, T* value) const
    {
        QString result = m_data.value(key);
        if(result.isNull())
            return false;

        return QnLexical::deserialize(result, value);
    }

    bool isEmpty() const {
        return m_data.isEmpty();
    }

private:
#ifdef _DEBUG /* QMap is easier to look through in debug. */
    QMap<QString, QString> m_data;
#else
    QHash<QString, QString> m_data;
#endif
};

// -------------------------------------------------------------------------- //
// QnAxisPtzController
// -------------------------------------------------------------------------- //
QnAxisPtzController::QnAxisPtzController(const QnPlAxisResourcePtr& resource):
    base_type(resource),
    m_resource(resource),
    m_capabilities(Qn::NoCapabilities)
{
    updateState();

    QnResourceData data = resource->resourceData();
    m_maxDeviceSpeed = QVector3D(
        data.value<qreal>(lit("axisMaxPanSpeed"), 100),
        data.value<qreal>(lit("axisMaxTiltSpeed"), 100),
        data.value<qreal>(lit("axisMaxZoomSpeed"), 100)
    );

    m_cacheUpdateTimer.invalidate();
}

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

void QnAxisPtzController::updateState() {
    QnAxisParameterMap params;
    if(
        !query(lit("param.cgi?action=list&group=Properties.PTZ"), 5, &params) ||
        !query(lit("param.cgi?action=list&group=PTZ"), 5, &params) ||
        !query(lit("param.cgi?action=list&group=Image"), 5, &params)
    ) {
        qnWarning("Could not initialize AXIS PTZ for camera %1.", m_resource->getName());
    }

    updateState(params);
}

void QnAxisPtzController::updateState(const QnAxisParameterMap& params)
{
    m_capabilities = 0;

    int channel = qMax(this->channel(), 1);

    QString ptzEnabled = params.value<QString>(lit("root.Properties.PTZ.PTZ"));
    if(ptzEnabled != lit("yes"))
        return;

    if(params.value<bool>(lit("root.PTZ.Various.V%1.Locked").arg(channel), false))
        return;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.ContinuousPan").arg(channel), false))
        m_capabilities |= Ptz::ContinuousPanCapability;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.ContinuousTilt").arg(channel), false))
        m_capabilities |= Ptz::ContinuousTiltCapability;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.ContinuousZoom").arg(channel), false))
        m_capabilities |= Ptz::ContinuousZoomCapability;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.AbsolutePan").arg(channel), false))
        m_capabilities |= Ptz::AbsolutePanCapability;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.AbsoluteTilt").arg(channel), false))
        m_capabilities |= Ptz::AbsoluteTiltCapability;

    if(params.value<bool>(lit("root.PTZ.Support.S%1.AbsoluteZoom").arg(channel), false))
        m_capabilities |= Ptz::AbsoluteZoomCapability;

    if (params.value<bool>(lit("root.PTZ.Support.S%1.RelativePan").arg(channel), false))
        m_capabilities |= Ptz::RelativePanCapability;

    if (params.value<bool>(lit("root.PTZ.Support.S%1.RelativeTilt").arg(channel), false))
        m_capabilities |= Ptz::RelativeTiltCapability;

    if (params.value<bool>(lit("root.PTZ.Support.S%1.RelativeZoom").arg(channel), false))
        m_capabilities |= Ptz::RelativeZoomCapability;

    if (params.value<bool>(lit("root.PTZ.Support.S%1.RelativeFocus").arg(channel), false))
        m_capabilities |= Ptz::RelativeFocusCapability;

    if (!params.value<bool>(lit("root.PTZ.Various.V%1.PanEnabled").arg(channel), true))
    {
        m_capabilities &= ~(Ptz::ContinuousPanCapability
            | Ptz::AbsolutePanCapability
            | Ptz::RelativePanCapability);
    }

    if (!params.value<bool>(lit("root.PTZ.Various.V%1.TiltEnabled").arg(channel), true))
    {
        m_capabilities &= ~(Ptz::ContinuousTiltCapability
            | Ptz::AbsoluteTiltCapability
            | Ptz::RelativeTiltCapability);
    }

    if (!params.value<bool>(lit("root.PTZ.Various.V%1.ZoomEnabled").arg(channel), true))
    {
        m_capabilities &= ~(Ptz::ContinuousZoomCapability
            | Ptz::AbsoluteZoomCapability
            | Ptz::RelativeZoomCapability);
    }

    if (!params.value<bool>(QString("root.PTZ.Various.V%1.FocusEnabled").arg(channel), true))
        m_capabilities &= ~Ptz::RelativeFocusCapability;

    /* Note that during continuous move axis takes care of image rotation automagically. */
    qreal rotation = params.value(lit("root.Image.I0.Appearance.Rotation"), 0.0); // TODO: #Elric I0? Not I%1?
    if(qFuzzyCompare(rotation, static_cast<qreal>(180.0)))
        m_flip = Qt::Vertical | Qt::Horizontal;
    m_capabilities |= Ptz::FlipPtzCapability;

    QnPtzLimits limits;
    if(
        params.value(lit("root.PTZ.Limit.L%1.MinPan").arg(channel), &limits.minPan) &&
        params.value(lit("root.PTZ.Limit.L%1.MaxPan").arg(channel), &limits.maxPan) &&
        params.value(lit("root.PTZ.Limit.L%1.MinTilt").arg(channel), &limits.minTilt) &&
        params.value(lit("root.PTZ.Limit.L%1.MaxTilt").arg(channel), &limits.maxTilt) &&
        params.value(lit("root.PTZ.Limit.L%1.MinFieldAngle").arg(channel), &limits.minFov) &&
        params.value(lit("root.PTZ.Limit.L%1.MaxFieldAngle").arg(channel), &limits.maxFov) &&
        limits.minPan <= limits.maxPan &&
        limits.minTilt <= limits.maxTilt &&
        limits.minFov <= limits.maxFov
    ) {
        m_rawLimits = limits;
        /* These are in 1/10th of a degree, so we convert them away. */
        limits.minFov /= 10.0;
        limits.maxFov /= 10.0;

        /* We don't want strange tilt angles supported by Axis's E-Flip. */
        limits.maxTilt = qMin(limits.maxTilt, 90.0);
        limits.minTilt = qMax(limits.minTilt, -90.0);

        /* Logical space should work now that we know logical limits. */
        m_capabilities |= Ptz::LogicalPositioningPtzCapability | Ptz::LimitsPtzCapability;
        m_limits = limits;

        /* Axis's zoom scale is linear in 35mm-equiv space. */
        m_35mmEquivToCameraZoom = QnLinearFunction(qDegreesTo35mmEquiv(limits.minFov), 9999, qDegreesTo35mmEquiv(limits.maxFov), 1);
        m_cameraTo35mmEquivZoom = m_35mmEquivToCameraZoom.inversed();
    } else {
        m_capabilities &= ~Ptz::AbsolutePtzCapabilities;
        m_capabilities &= ~Ptz::RelativePanTiltCapabilities;
    }

    if (params.value(QString("root.PTZ.Limit.L%1.MinZoom").arg(channel), &m_rawLimits.minFov)
        && params.value(QString("root.PTZ.Limit.L%1.MaxZoom").arg(channel), &m_rawLimits.maxFov))
    {
        m_capabilities &= ~Ptz::RelativeZoomCapability;
        m_rawLimits.minFov = 0;
        m_rawLimits.maxFov = 0;
    }

    if (params.value(QString("root.PTZ.Limit.L%1.MinFocus").arg(channel), &m_rawLimits.minFocus)
        && params.value(QString("root.PTZ.Limit.L%1.MaxFocus").arg(channel), &m_rawLimits.maxFocus))
    {
        m_capabilities &= ~Ptz::RelativeFocusCapability;
    }

    QByteArray body;
    if (query(lit("com/ptz.cgi?info=1"), &body))
    {
        for (auto line: body.split('\n'))
        {
            if (line.trimmed().startsWith("gotoserverpresetno"))
                m_capabilities |= (Ptz::PresetsPtzCapability | Ptz::NativePresetsPtzCapability);
        }
    }
}

CLSimpleHTTPClient *QnAxisPtzController::newHttpClient() const
{
    return new CLSimpleHTTPClient(
        m_resource->getHostAddress(),
        QUrl(m_resource->getUrl()).port(DEFAULT_AXIS_API_PORT),
        m_resource->getNetworkTimeout(),  // TODO: #Elric use int in getNetworkTimeout
        m_resource->getAuth()
    );
}

QByteArray QnAxisPtzController::getCookie() const
{
    QnMutexLocker locker( &m_mutex );
    return m_cookie;
}

void QnAxisPtzController::setCookie(const QByteArray& cookie)
{
    QnMutexLocker locker( &m_mutex );
    m_cookie = cookie;
}

bool QnAxisPtzController::queryInternal(const QString& request, QByteArray* body)
{
    QScopedPointer<CLSimpleHTTPClient> http(newHttpClient());

    QByteArray cookie = getCookie();

    for (int i = 0; i < 2; ++i)
    {
        if (m_resource->commonModule()->isNeedToStop())
            return false;

        if (!cookie.isEmpty())
            http->addHeader("Cookie", cookie);
        CLHttpStatus status = http->doGET(lit("axis-cgi/%1").arg(request).toLatin1());

        if (status == CL_HTTP_SUCCESS)
        {
            if(body) {
                QByteArray localBody;
                http->readAll(localBody);
                if(body) // TODO: #Elric why the double check?
                    *body = localBody;

                if(localBody.startsWith("Error:"))
                {
                    qnWarning("Failed to execute request '%1' for camera %2. Camera returned: %3.",
                        request, m_resource->getName(), localBody.mid(6));
                    return false;
                }
            }
            return true;
        }
        else if (status == CL_HTTP_REDIRECT && cookie.isEmpty())
        {
            cookie = http->header().value("Set-Cookie");
            cookie = cookie.split(';')[0];
            if (cookie.isEmpty())
                return false;
            setCookie(cookie);
        }
        else
        {
            qnWarning("Failed to execute request '%1' for camera %2. Result: %3.",
                request, m_resource->getName(), ::toString(status));
            return false;
        }

        /* If we do not return, repeat request with cookie on the second loop step. */
    }

    return false;
}

bool QnAxisPtzController::query(const QString& request, QByteArray* body) const
{
    const int channel = this->channel();
    const auto targetRequest = channel < 0
        ? request
        : request + lit("&camera=%1").arg(channel);

    auto nonConstThis = const_cast<QnAxisPtzController*>(this);
    return nonConstThis->queryInternal(targetRequest, body);
}

bool QnAxisPtzController::query(const QString& request, QnAxisParameterMap* params,
    QByteArray* body) const
{
    QByteArray localBody;
    if(!query(request, &localBody))
        return false;

    if(params)
    {
        QTextStream stream(&localBody, QIODevice::ReadOnly);
        while(true)
        {
            QString line = stream.readLine();
            if(line.isNull())
                break;

            int index = line.indexOf(L'=');
            if(index == -1)
                continue;

            params->insert(line.left(index), line.mid(index + 1));
        }
    }

    if(body)
        *body = localBody;

    return true;
}

bool QnAxisPtzController::query(const QString &request, int retries, QnAxisParameterMap *params,
    QByteArray *body) const
{
    QByteArray localBody;

    for(int i = 0; i < retries; i++)
    {
        if(!query(request, params, &localBody))
            return false;

        if(localBody.isEmpty())
            continue;

        if(body)
            *body = localBody;
        return true;
    }

    return false;
}

int QnAxisPtzController::channel() const
{
    int channelCount = m_resource->getMaxChannelsPhysical();
    return channelCount > 1 ? m_resource->getChannelNumAxis() : -1;
}

Ptz::Capabilities QnAxisPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return m_capabilities;
}

bool QnAxisPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    return query(lm("com/ptz.cgi?continuouspantiltmove=%1,%2&continuouszoommove=%3")
        .args(
            speedVector.pan * m_maxDeviceSpeed.x(),
            speedVector.tilt * m_maxDeviceSpeed.y(),
            speedVector.zoom * m_maxDeviceSpeed.z()));
}

bool QnAxisPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    return query(lm("com/ptz.cgi?pan=%1&tilt=%2&zoom=%3&speed=%4")
        .args(
            position.pan,
            position.tilt,
            m_35mmEquivToCameraZoom(qDegreesTo35mmEquiv(position.zoom)),
            speed * 100));
}

bool QnAxisPtzController::relativeMove(
    const nx::core::ptz::Vector& relativeMovementVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Relative movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    const auto axisRelativeMovementVector = toAxisRelativeMovement(
        relativeMovementVector,
        m_rawLimits);

    return query(lm("com/ptz.cgi?rpan=%1&rtilt=%2&rzoom=%3")
        .args(
            axisRelativeMovementVector.pan,
            axisRelativeMovementVector.tilt,
            axisRelativeMovementVector.zoom));
}

bool QnAxisPtzController::relativeFocus(
    qreal relativeFocusMovement,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Relative focus - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    ptz::Vector axisVector;
    axisVector.focus = relativeFocusMovement;
    axisVector = toAxisRelativeMovement(axisVector, m_rawLimits);

    return query(lm("com/ptz.cgi?rfocus=%1").args(axisVector.focus));
}

bool QnAxisPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options)  const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting current position - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    QnAxisParameterMap params;
    if(!query(lit("com/ptz.cgi?query=position"), &params))
        return false;

    qreal pan, tilt, zoom;
    if(params.value(lit("pan"), &pan) && params.value(lit("tilt"), &tilt) && params.value(lit("zoom"), &zoom))
    {
        outPosition->pan = pan;
        outPosition->tilt = tilt;
        outPosition->zoom = q35mmEquivToDegrees(m_cameraTo35mmEquivZoom(zoom));
        return true;
    }
    else
    {
        NX_WARNING(
            this,
            lm("Failed to get PTZ position from camera %1. Malformed response.")
                .arg(m_resource->getName()));

        return false;
    }
}

bool QnAxisPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits *limits,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting limits - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnAxisPtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(m_resource->getName(), m_resource->getId()));

        return false;
    }

    *flip = m_flip;
    return true;
}

bool QnAxisPtzController::getPresets(QnPtzPresetList* presets) const
{
    if (!(m_capabilities & Ptz::PresetsPtzCapability))
        return base_type::getPresets(presets);

    if (!m_cacheUpdateTimer.isValid() || m_cacheUpdateTimer.elapsed() > CACHE_UPDATE_TIMEOUT)
    {
        QByteArray body;
        bool rez = query(lit("com/ptz.cgi?query=presetposcam"), &body);
        if (!rez)
            return rez;
        QnMutexLocker lock(&m_mutex);
        m_cachedData.clear();
        for(QByteArray line: body.split(L'\n').mid(1))
        {
            line = line.trimmed();
            QList<QByteArray> params = line.split(L'=');
            if (params.size() == 2)
                m_cachedData.insert(QString::fromUtf8(params[0]), QString::fromUtf8(params[1]));
        }
        m_cacheUpdateTimer.restart();
    }

    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_cachedData.begin(); itr != m_cachedData.end(); ++itr)
        presets->push_back(QnPtzPreset(itr.key(), itr.value()));
    return true;
}

int extractPresetNum(const QString& presetId)
{
    QString result;
    for (int i = 0; i < presetId.size(); ++i)
    {
        if (presetId.at(i) >= L'0' && presetId.at(i) <= L'9')
            result += presetId.at(i);
    }
    return result.toInt();
}

bool QnAxisPtzController::activatePreset(const QString& presetId, qreal speed)
{
    if (!(m_capabilities & Ptz::PresetsPtzCapability))
        return base_type::activatePreset(presetId, speed);

    return query(lit("com/ptz.cgi?gotoserverpresetno=%1").arg(extractPresetNum(presetId)));
}

bool QnAxisPtzController::createPreset(const QnPtzPreset& preset)
{
    if (!(m_capabilities & Ptz::PresetsPtzCapability))
        return base_type::createPreset(preset);

    m_cacheUpdateTimer.invalidate();
    return query(lit("com/ptz.cgi?setserverpresetname=%1").arg(preset.name));
}

bool QnAxisPtzController::updatePreset(const QnPtzPreset& preset)
{
    if (!(m_capabilities & Ptz::PresetsPtzCapability))
        return base_type::updatePreset(preset);
    m_cacheUpdateTimer.invalidate();
    return removePreset(preset.id) && createPreset(preset);
}

bool QnAxisPtzController::removePreset(const QString& presetId)
{
    if (!(m_capabilities & Ptz::PresetsPtzCapability))
        return base_type::removePreset(presetId);
    m_cacheUpdateTimer.invalidate();
    return query(lit("com/ptz.cgi?removeserverpresetno=%1").arg(extractPresetNum(presetId)));
}

#endif // ENABLE_AXIS
