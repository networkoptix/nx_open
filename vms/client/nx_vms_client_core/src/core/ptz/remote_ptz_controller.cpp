#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/request_param.h>
#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>

using namespace nx::core;

namespace {
    static constexpr nx::core::ptz::Options kDefaultOptions = {nx::core::ptz::Type::operational};
} // namespace

QnRemotePtzController::QnRemotePtzController(const QnVirtualCameraResourcePtr& camera):
    base_type(camera),
    m_camera(camera),
    m_sequenceId(QnUuid::createUuid()),
    m_sequenceNumber(1)
{
}

QnRemotePtzController::~QnRemotePtzController()
{
}

Ptz::Capabilities QnRemotePtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    Ptz::Capabilities result = m_camera->getPtzCapabilities(options.type);
    if (!result)
        return Ptz::NoPtzCapabilities;

    if (result.testFlag(Ptz::VirtualPtzCapability))
        return Ptz::NoPtzCapabilities; /* Can't have remote virtual PTZ. */

    result |= Ptz::AsynchronousPtzCapability;
    result &= ~(Ptz::FlipPtzCapability | Ptz::LimitsPtzCapability);
    return result;
}

QnMediaServerResourcePtr QnRemotePtzController::getMediaServer() const
{
    auto server = m_camera->getParentServer();
    if (!NX_ASSERT(server))
        return {};

    return server;
}

bool QnRemotePtzController::isPointless(
    Qn::PtzCommand command,
    const nx::core::ptz::Options& options) const
{
    if (!getMediaServer())
        return true;

    const Qn::ResourceStatus status = m_camera->getStatus();
    if (status == Qn::Unauthorized || status == Qn::Offline)
        return true;

    // Skip check until `configurationalPtzCapabilities ` property is set on the server side.
    if (options.type == nx::core::ptz::Type::configurational)
        return false;

    return !base_type::supports(command, options);
}

int QnRemotePtzController::nextSequenceNumber()
{
    return m_sequenceNumber.fetchAndAddOrdered(1);
}

// Current PTZ API returns a pair of {Qn::PtzCommand command, QVariant data} for each request.
// This function helps to generate a proper "QVariant data" value.
template<class T>
QnRemotePtzController::PtzDeserializationHelper makeDeserializationHelper()
{
    return [](const QnJsonRestResult& data) -> QVariant
        {
            T value = data.deserialized<T>();
            return QVariant::fromValue(value);
        };
}

// Some PTZ functions embed request arguments to a response. This function helps to store this
// input argument and return it instead of server response.
template<class T>
QnRemotePtzController::PtzDeserializationHelper makeInputHelper(const T& data)
{
    auto response = QVariant::fromValue<T>(data);
    return [response](const QnJsonRestResult& /*data*/) -> QVariant
        {
            return response;
        };
}

nx::network::http::HttpHeaders makeDefaultHeaders()
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");

    return headers;
}

bool QnRemotePtzController::sendRequest(
    Qn::PtzCommand command,
    const QnRequestParamList& baseParams,
    const nx::Buffer& body,
    const nx::core::ptz::Options& options,
    PtzDeserializationHelper helper) const
{
    NX_VERBOSE(this, "Sending command %1 to %2", QnLexical::serialized(command), m_camera);

    auto server = getMediaServer();
    if (!NX_ASSERT(server))
        return false;

    if (isPointless(command, options))
    {
        NX_VERBOSE(this, "Sending canceled as the command is pointless");
        return false;
    }

    auto connection = server->restConnection();
    if (NX_ASSERT(connection))
    {
        QnRequestParamList params = baseParams;
        if (server->getVersion() < nx::utils::SoftwareVersion(3, 0))
            params.insert("resourceId", QnLexical::serialized(m_camera->getUniqueId()));

        params.insert("command", QnLexical::serialized(command));
        params.insert("cameraId", m_camera->getId().toString());

        if (!helper)
            helper = [](const QnJsonRestResult& /*data*/) { return QVariant(); };

        PtzServerCallback callback = nx::utils::guarded(this,
            [this, command, deserializer=std::move(helper)](
                bool /*success*/, rest::Handle /*handle*/, const QnJsonRestResult& data)
            {
                QVariant value = deserializer(data);
                auto notConstThis = const_cast<QnRemotePtzController*>(this);
                emit notConstThis->finished(command, value);
            });

        connection->postJsonResult("/api/ptz", params, body, std::move(callback));
    }
    return true;
}

bool QnRemotePtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("xSpeed", QnLexical::serialized(speed.pan));
    params.insert("ySpeed", QnLexical::serialized(speed.tilt));
    params.insert("zSpeed", QnLexical::serialized(speed.zoom));
    params.insert("rotationSpeed", QnLexical::serialized(speed.rotation));
    params.insert("type", QnLexical::serialized(options.type));
    params.insert("sequenceId", m_sequenceId.toString());
    params.insert("sequenceNumber", QString::number(nextSequenceNumber()));
    return sendRequest(Qn::ContinuousMovePtzCommand, params, nx::Buffer(), options);
}

bool QnRemotePtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", QnLexical::serialized(options.type));
    return sendRequest(Qn::ContinuousFocusPtzCommand, params, nx::Buffer(), options);
}

bool QnRemotePtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("xPos", QnLexical::serialized(position.pan));
    params.insert("yPos", QnLexical::serialized(position.tilt));
    params.insert("zPos", QnLexical::serialized(position.zoom));
    params.insert("rotaion", QnLexical::serialized(position.rotation));
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", QnLexical::serialized(options.type));
    params.insert("sequenceId", m_sequenceId.toString());
    params.insert("sequenceNumber", QString::number(nextSequenceNumber()));
    return sendRequest(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), params,
        nx::Buffer(), options);
}

bool QnRemotePtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("aspectRatio", QnLexical::serialized(aspectRatio));
    params.insert("viewportTop", QnLexical::serialized(viewport.top()));
    params.insert("viewportLeft", QnLexical::serialized(viewport.left()));
    params.insert("viewportBottom", QnLexical::serialized(viewport.bottom()));
    params.insert("viewportRight", QnLexical::serialized(viewport.right()));
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", QnLexical::serialized(options.type));
    params.insert("sequenceId", m_sequenceId.toString());
    params.insert("sequenceNumber", QString::number(nextSequenceNumber()));
    return sendRequest(Qn::ViewportMovePtzCommand, params, nx::Buffer(), options);
}

bool QnRemotePtzController::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("pan", QnLexical::serialized(direction.pan));
    params.insert("tilt", QnLexical::serialized(direction.tilt));
    params.insert("rotation", QnLexical::serialized(direction.rotation));
    params.insert("zoom", QnLexical::serialized(direction.zoom));
    params.insert("type", QnLexical::serialized(options.type));
    params.insert("sequenceId", m_sequenceId.toString());
    params.insert("sequenceNumber", QString::number(nextSequenceNumber()));
    return sendRequest(Qn::RelativeMovePtzCommand, params, nx::Buffer(), options);
}

bool QnRemotePtzController::relativeFocus(qreal direction, const ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("focus", QnLexical::serialized(direction));
    params.insert("type", QnLexical::serialized(options.type));
    return sendRequest(Qn::RelativeFocusPtzCommand, params, nx::Buffer(), options);
}

bool QnRemotePtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* /*position*/,
    const nx::core::ptz::Options& options) const
{
    QnRequestParamList params;
    params.insert("type", QnLexical::serialized(options.type));

    auto helper = makeDeserializationHelper<QVector3D>();
    auto command = spaceCommand(Qn::GetDevicePositionPtzCommand, space);
    return sendRequest(command, params, nx::Buffer(), options, helper);
}

bool QnRemotePtzController::getLimits(
    Qn::PtzCoordinateSpace,
    QnPtzLimits* /*limits*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnRemotePtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset& preset)
{
    QnRequestParamList params;
    params.insert("presetName", preset.name);
    params.insert("presetId", preset.id);
    auto helper = makeInputHelper(preset);
    return sendRequest(Qn::CreatePresetPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset& preset)
{
    QnRequestParamList params;
    params.insert("presetName", preset.name);
    params.insert("presetId", preset.id);
    auto helper = makeInputHelper(preset);
    return sendRequest(Qn::UpdatePresetPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::removePreset(const QString& presetId)
{
    QnRequestParamList params;
    params.insert("presetId", presetId);
    auto helper = makeInputHelper(presetId);
    return sendRequest(Qn::RemovePresetPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::activatePreset(const QString& presetId, qreal speed)
{
    QnRequestParamList params;
    params.insert("presetId", presetId);
    params.insert("speed", QnLexical::serialized(speed));
    auto helper = makeInputHelper(presetId);
    return sendRequest(Qn::ActivatePresetPtzCommand, params, nx::Buffer(), kDefaultOptions,
        helper);
}

bool QnRemotePtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    QnRequestParamList params;
    auto helper = makeDeserializationHelper<QnPtzPresetList>();
    return sendRequest(Qn::GetPresetsPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::createTour(
    const QnPtzTour& tour)
{
    QnRequestParamList params;
    auto helper = makeInputHelper(tour);
    return sendRequest(Qn::CreateTourPtzCommand, params, QJson::serialized(tour), kDefaultOptions,
        helper);
}

bool QnRemotePtzController::removeTour(const QString& tourId)
{
    QnRequestParamList params;
    params.insert("tourId", tourId);
    auto helper = makeInputHelper(tourId);
    return sendRequest(Qn::RemoveTourPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::activateTour(const QString& tourId)
{
    QnRequestParamList params;
    params.insert("tourId", tourId);
    auto helper = makeInputHelper(tourId);
    return sendRequest(Qn::ActivateTourPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::getTours(QnPtzTourList* /*tours*/) const
{
    QnRequestParamList params;
    auto helper = makeDeserializationHelper<QnPtzTourList>();
    return sendRequest(Qn::GetToursPtzCommand, params, nx::Buffer(), kDefaultOptions, helper);
}

bool QnRemotePtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    QnRequestParamList params;
    auto helper = makeDeserializationHelper<QnPtzObject>();
    return sendRequest(Qn::GetActiveObjectPtzCommand, params, nx::Buffer(), kDefaultOptions,
        helper);
}

bool QnRemotePtzController::updateHomeObject(const QnPtzObject& homePosition)
{
    QnRequestParamList params;
    params.insert("objectType", QnLexical::serialized(homePosition.type));
    params.insert("objectId", homePosition.id);
    auto helper = makeInputHelper(homePosition);
    return sendRequest(Qn::UpdateHomeObjectPtzCommand, params, nx::Buffer(), kDefaultOptions,
        helper);
}

bool QnRemotePtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    QnRequestParamList params;
    auto helper = makeDeserializationHelper<QnPtzObject>();
    return sendRequest(Qn::GetHomeObjectPtzCommand, params, nx::Buffer(), kDefaultOptions,
        helper);
}

bool QnRemotePtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*auxiliaryTraits*/,
    const nx::core::ptz::Options& options) const
{
    QnRequestParamList params;
    params.insert("type", QnLexical::serialized(options.type));
    auto helper = makeDeserializationHelper<QnPtzAuxiliaryTraitList>();
    return sendRequest(Qn::GetAuxiliaryTraitsPtzCommand, params, nx::Buffer(), options, helper);
}

bool QnRemotePtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    QnRequestParamList params;
    params.insert("trait", QnLexical::serialized(trait));
    params.insert("data", data);
    params.insert("type", QnLexical::serialized(options.type));
    auto helper = makeInputHelper(trait);
    return sendRequest(Qn::RunAuxiliaryCommandPtzCommand, params, nx::Buffer(), options, helper);
}

bool QnRemotePtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* /*data*/,
    const nx::core::ptz::Options& options) const
{
    QnRequestParamList params;
    params.insert("query", QnLexical::serialized(query));
    params.insert("type", QnLexical::serialized(options.type));
    auto helper = makeDeserializationHelper<QnPtzData>();
    return sendRequest(Qn::GetDataPtzCommand, params, nx::Buffer(), options, helper);
}
