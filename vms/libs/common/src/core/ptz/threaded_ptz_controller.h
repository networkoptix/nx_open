#pragma once

#include <core/ptz/proxy_ptz_controller.h>

class QThreadPool;
class QnThreadedPtzControllerPrivate;

class QnPtzCommandBase: public QObject
{
    Q_OBJECT

signals:
    void finished(Qn::PtzCommand command, const QVariant& data);
};

class QnThreadedPtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    QnThreadedPtzController(
        const QnPtzControllerPtr& baseController,
        QThreadPool* threadPool);
    virtual ~QnThreadedPtzController();

    static bool extends(Ptz::Capabilities capabilities);

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

    virtual bool getAuxilaryTraits(
        QnPtzAuxilaryTraitList* auxilaryTraits,
        const nx::core::ptz::Options& options) const override;

    virtual bool runAuxilaryCommand(
        const QnPtzAuxilaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options) override;

    virtual bool getData(
        Qn::PtzDataFields query,
        QnPtzData* data,
        const nx::core::ptz::Options& options) const override;

private:
    template<class Functor>
    void runCommand(Qn::PtzCommand command, const Functor& functor) const;

private:
    QThreadPool* m_threadPool;
};
