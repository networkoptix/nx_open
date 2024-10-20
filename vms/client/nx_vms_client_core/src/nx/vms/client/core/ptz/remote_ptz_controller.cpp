// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/system_context.h>

using namespace nx::vms::common::ptz;

namespace nx::vms::client::core {
namespace ptz {

namespace {

static constexpr Options kDefaultOptions = {Type::operational};

} // namespace

RemotePtzController::RemotePtzController(const QnVirtualCameraResourcePtr& camera):
    base_type(camera),
    m_camera(camera),
    m_sequenceId(nx::Uuid::createUuid()),
    m_sequenceNumber(1)
{
}

RemotePtzController::~RemotePtzController()
{
}

Ptz::Capabilities RemotePtzController::getCapabilities(
    const Options& options) const
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

bool RemotePtzController::isPointless(
    Command command,
    const Options& options) const
{
    if (!m_camera->getParentServer())
        return true;

    const nx::vms::api::ResourceStatus status = m_camera->getStatus();
    if (status == nx::vms::api::ResourceStatus::unauthorized || status == nx::vms::api::ResourceStatus::offline)
        return true;

    // Skip check until `configurationalPtzCapabilities ` property is set on the server side.
    if (options.type == Type::configurational)
        return false;

    return !base_type::supports(command, options);
}

int RemotePtzController::nextSequenceNumber()
{
    return m_sequenceNumber.fetchAndAddOrdered(1);
}

// Current PTZ API returns a pair of {Command command, QVariant data} for each request.
// This function helps to generate a proper "QVariant data" value.
template<class T>
RemotePtzController::PtzDeserializationHelper makeDeserializationHelper()
{
    return [](const nx::network::rest::JsonResult& data) -> QVariant
        {
            T value = data.deserialized<T>();
            return QVariant::fromValue(value);
        };
}

// Some PTZ functions embed request arguments to a response. This function helps to store this
// input argument and return it instead of server response.
template<class T>
RemotePtzController::PtzDeserializationHelper makeInputHelper(const T& data)
{
    auto response = QVariant::fromValue<T>(data);
    return [response](const nx::network::rest::JsonResult& /*data*/) -> QVariant
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

bool RemotePtzController::sendRequest(
    Command command,
    const nx::network::rest::Params& baseParams,
    const QByteArray& body,
    const Options& options,
    PtzDeserializationHelper helper) const
{
    NX_VERBOSE(this, "Sending command %1 to %2", command, m_camera);

    auto server = m_camera->getParentServer();

    // It is possible that a camera is received earlier than the camera's server (merging process).
    if (!server)
        return false;

    if (isPointless(command, options))
    {
        NX_VERBOSE(this, "Sending canceled as the command is pointless");
        return false;
    }

    auto systemContext = SystemContext::fromResource(m_camera);
    if (!NX_ASSERT(systemContext))
        return false;

    auto api = systemContext->connectedServerApi();
    if (!api)
        return false;

    nx::network::rest::Params params = baseParams;

    params.insert("command", nx::reflect::toString(command));
    params.insert("cameraId", m_camera->getId());

    if (!helper)
        helper = [](const nx::network::rest::JsonResult& /*data*/) { return QVariant(); };

    PtzServerCallback callback = nx::utils::guarded(this,
        [this, command, deserializer=std::move(helper)](
            bool /*success*/, rest::Handle /*handle*/, const nx::network::rest::JsonResult& data)
        {
            QVariant value = deserializer(data);
            auto notConstThis = const_cast<RemotePtzController*>(this);
            emit notConstThis->finished(command, value);
        });

    api->postJsonResult(
        "/api/ptz",
        params,
        body,
        std::move(callback),
        this->thread(),
        /*timeouts*/ {},
        server->getId());

    return true;
}

bool RemotePtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("xSpeed", QnLexical::serialized(speed.pan));
    params.insert("ySpeed", QnLexical::serialized(speed.tilt));
    params.insert("zSpeed", QnLexical::serialized(speed.zoom));
    params.insert("rotationSpeed", QnLexical::serialized(speed.rotation));
    params.insert("type", nx::reflect::toString(options.type));
    params.insert("sequenceId", m_sequenceId);
    params.insert("sequenceNumber", nextSequenceNumber());
    return sendRequest(Command::continuousMove, params, QByteArray(), options);
}

bool RemotePtzController::continuousFocus(
    qreal speed,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", options.type);
    return sendRequest(Command::continuousFocus, params, QByteArray(), options);
}

bool RemotePtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("xPos", QnLexical::serialized(position.pan));
    params.insert("yPos", QnLexical::serialized(position.tilt));
    params.insert("zPos", QnLexical::serialized(position.zoom));
    params.insert("rotaion", QnLexical::serialized(position.rotation));
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", options.type);
    params.insert("sequenceId", m_sequenceId);
    params.insert("sequenceNumber", nextSequenceNumber());
    return sendRequest(spaceCommand(Command::absoluteDeviceMove, space), params,
        QByteArray(), options);
}

bool RemotePtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("aspectRatio", QnLexical::serialized(aspectRatio));
    params.insert("viewportTop", QnLexical::serialized(viewport.top()));
    params.insert("viewportLeft", QnLexical::serialized(viewport.left()));
    params.insert("viewportBottom", QnLexical::serialized(viewport.bottom()));
    params.insert("viewportRight", QnLexical::serialized(viewport.right()));
    params.insert("speed", QnLexical::serialized(speed));
    params.insert("type", options.type);
    params.insert("sequenceId", m_sequenceId);
    params.insert("sequenceNumber", nextSequenceNumber());
    return sendRequest(Command::viewportMove, params, QByteArray(), options);
}

bool RemotePtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("pan", QnLexical::serialized(direction.pan));
    params.insert("tilt", QnLexical::serialized(direction.tilt));
    params.insert("rotation", QnLexical::serialized(direction.rotation));
    params.insert("zoom", QnLexical::serialized(direction.zoom));
    params.insert("type", options.type);
    params.insert("sequenceId", m_sequenceId);
    params.insert("sequenceNumber", nextSequenceNumber());
    return sendRequest(Command::relativeMove, params, QByteArray(), options);
}

bool RemotePtzController::relativeFocus(qreal direction, const Options& options)
{
    nx::network::rest::Params params;
    params.insert("focus", QnLexical::serialized(direction));
    params.insert("type", options.type);
    return sendRequest(Command::relativeFocus, params, QByteArray(), options);
}

bool RemotePtzController::getPosition(
    Vector* /*position*/,
    CoordinateSpace space,
    const Options& options) const
{
    nx::network::rest::Params params;
    params.insert("type", options.type);

    const auto deserializeVector3D =
        [](const nx::network::rest::JsonResult& data)
        {
            const QJsonObject obj = data.reply.toObject();
            QVector3D value(
                obj.value("x").toDouble(),
                obj.value("y").toDouble(),
                obj.value("z").toDouble());
            return QVariant::fromValue(value);
        };

    auto command = spaceCommand(Command::getDevicePosition, space);
    return sendRequest(command, params, QByteArray(), options, deserializeVector3D);
}

bool RemotePtzController::getLimits(
    QnPtzLimits* /*limits*/,
    CoordinateSpace,
    const Options& /*options*/) const
{
    return false;
}

bool RemotePtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const Options& /*options*/) const
{
    return false;
}

bool RemotePtzController::createPreset(const QnPtzPreset& preset)
{
    nx::network::rest::Params params;
    params.insert("presetName", preset.name);
    params.insert("presetId", preset.id);
    auto helper = makeInputHelper(preset);
    return sendRequest(Command::createPreset, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::updatePreset(const QnPtzPreset& preset)
{
    nx::network::rest::Params params;
    params.insert("presetName", preset.name);
    params.insert("presetId", preset.id);
    auto helper = makeInputHelper(preset);
    return sendRequest(Command::updatePreset, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::removePreset(const QString& presetId)
{
    nx::network::rest::Params params;
    params.insert("presetId", presetId);
    auto helper = makeInputHelper(presetId);
    return sendRequest(Command::removePreset, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::activatePreset(const QString& presetId, qreal speed)
{
    nx::network::rest::Params params;
    params.insert("presetId", presetId);
    params.insert("speed", QnLexical::serialized(speed));
    auto helper = makeInputHelper(presetId);
    return sendRequest(Command::activatePreset, params, QByteArray(), kDefaultOptions,
        helper);
}

bool RemotePtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    nx::network::rest::Params params;
    auto helper = makeDeserializationHelper<QnPtzPresetList>();
    return sendRequest(Command::getPresets, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::createTour(
    const QnPtzTour& tour)
{
    nx::network::rest::Params params;
    auto helper = makeInputHelper(tour);
    return sendRequest(Command::createTour, params, QJson::serialized(tour), kDefaultOptions,
        helper);
}

bool RemotePtzController::removeTour(const QString& tourId)
{
    nx::network::rest::Params params;
    params.insert("tourId", tourId);
    auto helper = makeInputHelper(tourId);
    return sendRequest(Command::removeTour, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::activateTour(const QString& tourId)
{
    nx::network::rest::Params params;
    params.insert("tourId", tourId);
    auto helper = makeInputHelper(tourId);
    return sendRequest(Command::activateTour, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::getTours(QnPtzTourList* /*tours*/) const
{
    nx::network::rest::Params params;
    auto helper = makeDeserializationHelper<QnPtzTourList>();
    return sendRequest(Command::getTours, params, QByteArray(), kDefaultOptions, helper);
}

bool RemotePtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    nx::network::rest::Params params;
    auto helper = makeDeserializationHelper<QnPtzObject>();
    return sendRequest(Command::getActiveObject, params, QByteArray(), kDefaultOptions,
        helper);
}

bool RemotePtzController::updateHomeObject(const QnPtzObject& homePosition)
{
    nx::network::rest::Params params;
    params.insert("objectType", homePosition.type);
    params.insert("objectId", homePosition.id);
    auto helper = makeInputHelper(homePosition);
    return sendRequest(Command::updateHomeObject, params, QByteArray(), kDefaultOptions,
        helper);
}

bool RemotePtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    nx::network::rest::Params params;
    auto helper = makeDeserializationHelper<QnPtzObject>();
    return sendRequest(Command::getHomeObject, params, QByteArray(), kDefaultOptions,
        helper);
}

bool RemotePtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*auxiliaryTraits*/,
    const Options& options) const
{
    nx::network::rest::Params params;
    params.insert("type", options.type);
    auto helper = makeDeserializationHelper<QnPtzAuxiliaryTraitList>();
    return sendRequest(Command::getAuxiliaryTraits, params, QByteArray(), options, helper);
}

bool RemotePtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    nx::network::rest::Params params;
    params.insert("trait", QnLexical::serialized(trait));
    params.insert("data", data);
    params.insert("type", options.type);
    auto helper = makeInputHelper(trait);
    return sendRequest(Command::runAuxiliaryCommand, params, QByteArray(), options, helper);
}

bool RemotePtzController::getData(
    QnPtzData* /*data*/,
    DataFields query,
    const Options& options) const
{
    nx::network::rest::Params params;
    params.insert("query", query);
    params.insert("type", options.type);
    auto helper = makeDeserializationHelper<QnPtzData>();
    return sendRequest(Command::getData, params, QByteArray(), options, helper);
}

} // namespace ptz
} // namespace nx::vms::client::core
