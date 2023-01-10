// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/proxy_ptz_controller.h>

class QThreadPool;
class QnThreadedPtzControllerPrivate;

class AsyncPtzCommandExecutorInterface: public QObject
{
    Q_OBJECT

signals:
    void finished(nx::vms::common::ptz::Command command, const QVariant& data);
};

class QnThreadedPtzController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    QnThreadedPtzController(
        const QnPtzControllerPtr& baseController,
        QThreadPool* threadPool);
    virtual ~QnThreadedPtzController();

    static bool extends(Ptz::Capabilities capabilities);

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

private:
    template<typename ResultValue, typename MethodPointer, typename... Args>
    bool executeCommand(
        Command command,
        ResultValue result,
        MethodPointer method,
        Options internalOptions,
        Args&&... args) const;

    template<typename ResultType, typename MethodPointer, typename... Args>
    bool requestData(
        Command command,
        MethodPointer method,
        Options internalOptions,
        Args&&... args) const;

    using PtzCommandFunctor = std::function<QVariant(const QnPtzControllerPtr& controller)>;
    void callThreaded(Command command, const PtzCommandFunctor& functor) const;

private:
    QThreadPool* m_threadPool;
};
