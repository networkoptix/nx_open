#pragma once

#include <core/ptz/abstract_ptz_controller.h>

/**
 * A simple controller that proxies all requests into another controller.
 */
class QnProxyPtzController: public QnAbstractPtzController
{
    Q_OBJECT
    using base_type = QnAbstractPtzController;

public:
    QnProxyPtzController(const QnPtzControllerPtr& controller = QnPtzControllerPtr());

    void setBaseController(const QnPtzControllerPtr& controller);
    QnPtzControllerPtr baseController() const;

public: // Overrides section
    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool continuousMove(const nx::core::ptz::Vector& speed) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, nx::core::ptz::Vector* position) const override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

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

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const override;
    virtual bool runAuxilaryCommand(const QnPtzAuxilaryTrait& trait, const QString& data) override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData* data) const override;

signals:
    void finishedLater(Qn::PtzCommand command, const QVariant& data);
    void baseControllerChanged();

protected:
    virtual void baseFinished(Qn::PtzCommand command, const QVariant& data);
    virtual void baseChanged(Qn::PtzDataFields fields);

private:
    QnPtzControllerPtr m_controller;
};
