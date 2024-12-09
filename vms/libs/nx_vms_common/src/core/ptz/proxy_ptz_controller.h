// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/abstract_ptz_controller.h>

/**
 * A simple controller that proxies all requests into another controller.
 */
class NX_VMS_COMMON_API QnProxyPtzController: public QnAbstractPtzController
{
    Q_OBJECT
    using base_type = QnAbstractPtzController;

public:
    QnProxyPtzController(const QnPtzControllerPtr& controller = QnPtzControllerPtr());

    void setBaseController(const QnPtzControllerPtr& controller);
    QnPtzControllerPtr baseController() const;

public: // Overrides section
    virtual void initialize() override;

    virtual void invalidate() override;

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
    virtual std::optional<QnPtzTour> getActiveTour() override;
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

    virtual QnResourcePtr resource() const override;

signals:
    void finishedLater(Command command, const QVariant& data);
    void baseControllerChanged();

protected:
    virtual void baseFinished(Command command, const QVariant& data);
    virtual void baseChanged(DataFields fields);

private:
    QnPtzControllerPtr m_controller;
    bool m_isInitialized = false;
};
