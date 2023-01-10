// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/abstract_ptz_controller.h>

class NX_VMS_COMMON_API QnBasicPtzController: public QnAbstractPtzController
{
    using base_type = QnAbstractPtzController;

public:
    QnBasicPtzController(const QnResourcePtr &resource);
    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(
        const Vector& speed,
        const Options& options) override;

    virtual bool continuousFocus(qreal speed, const Options& options) override;

    virtual bool absoluteMove(
        CoordinateSpace space,
        const Vector& position,
        qreal speed,
        const Options& options) override;

    virtual bool relativeMove(
        const Vector& direction,
        const Options& options) override;

    virtual bool relativeFocus(qreal direction, const Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const Options& options) override;

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
};
