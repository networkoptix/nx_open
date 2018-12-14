#pragma once

#include <QtCore/QAtomicInt>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

class QnRemotePtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnRemotePtzController(const QnVirtualCameraResourcePtr& camera);
    virtual ~QnRemotePtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

    virtual bool continuousFocus(
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* position,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const nx::core::ptz::Options& options) const override;

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
        const nx::core::ptz::Options& options) const override;

    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options) override;

    virtual bool getData(
        Qn::PtzDataFields query,
        QnPtzData* data,
        const nx::core::ptz::Options& options) const override;

private slots:
    void at_replyReceived(int status, const QVariant& reply, int handle);

private:
    bool isPointless(
        Qn::PtzCommand command,
        const nx::core::ptz::Options& options);

    int nextSequenceNumber();
    QnMediaServerResourcePtr getMediaServer() const;

private:
    // TODO: #ynikitenkov move to private
    struct PtzCommandData
    {
        PtzCommandData():
            command(Qn::InvalidPtzCommand)
        {
        }

        PtzCommandData(Qn::PtzCommand command, const QVariant& value):
            command(command),
            value(value)
        {
        }

        Qn::PtzCommand command;
        QVariant value;
    };

    QnVirtualCameraResourcePtr m_camera;
    QnUuid m_sequenceId;
    QAtomicInt m_sequenceNumber;

    mutable QnMutex m_mutex;
    QHash<int, PtzCommandData> m_dataByHandle;
};

