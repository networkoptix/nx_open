// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAtomicInt>

#include <api/server_rest_connection_fwd.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::network::rest { struct JsonResult; }
namespace nx::network::rest { class Params; }

namespace nx::vms::client::core {
namespace ptz {

class NX_VMS_CLIENT_CORE_API RemotePtzController:
    public QnAbstractPtzController
{
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    RemotePtzController(const QnVirtualCameraResourcePtr& camera);
    virtual ~RemotePtzController();

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(
        const Vector& speed,
        const Options& options) override;

    virtual bool continuousFocus(
        qreal speed,
        const Options& options) override;

    virtual bool absoluteMove(
        CoordinateSpace space,
        const Vector& position,
        qreal speed,
        const Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const Options& options) override;

    virtual bool relativeMove(
        const Vector& direction,
        const Options& options) override;

    virtual bool relativeFocus(qreal direction, const Options& options) override;

    virtual bool getPosition(
        Vector* position,
        CoordinateSpace space,
        const Options& options) const override;

    virtual bool getLimits(
        QnPtzLimits* limits,
        CoordinateSpace space,
        const Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const Options& options) const override;

    virtual bool createPreset(const QnPtzPreset& preset) override;

    virtual bool updatePreset(const QnPtzPreset& preset) override;

    virtual bool removePreset(const QString& presetId) override;

    virtual bool activatePreset(const QString& presetId, qreal speed) override;

    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool createTour(const QnPtzTour& tour) override;

    virtual bool removeTour(const QString& tourId) override;

    virtual bool activateTour(const QString& tourId) override;

    virtual bool getTours(QnPtzTourList* tours) const override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;

    virtual bool updateHomeObject(const QnPtzObject& homeObject) override;

    virtual bool getHomeObject(QnPtzObject* homeObject) const override;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList* auxiliaryTraits,
        const Options& options) const override;

    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait& trait,
        const QString& data,
        const Options& options) override;

    virtual bool getData(
        QnPtzData* data,
        DataFields query,
        const Options& options) const override;

    using PtzServerCallback = std::function<
        void (bool success, rest::Handle handle, const nx::network::rest::JsonResult& data)>;

    using PtzDeserializationHelper = std::function<QVariant (const nx::network::rest::JsonResult& data)>;

    /**
     * Helper method to send request and get a proper response.
     * @param command - PTZ command.
     * @param params - all ptz parameters are stored there.
     * @param body - request body
     * @param options - additional options.
     * @param responseHelper - lambda to help deserialize QJsonValue to desired datatype.
     * @return true if request was sent.
     *
     * PtzController will "emit finish(command, response)" signal when server responds.
     */
    bool sendRequest(
        Command command,
        const nx::network::rest::Params& params,
        const QByteArray& body,
        const Options& options,
        PtzDeserializationHelper responseHelper = {}) const;

private:
    bool isPointless(
        Command command,
        const Options& options) const;

    int nextSequenceNumber();

private:
    QnVirtualCameraResourcePtr m_camera;
    nx::Uuid m_sequenceId;
    QAtomicInt m_sequenceNumber;

    mutable nx::Mutex m_mutex;
};

} // namespace ptz
} // namespace nx::vms::client::core
