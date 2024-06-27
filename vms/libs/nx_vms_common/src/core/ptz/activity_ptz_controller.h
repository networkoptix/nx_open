// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/proxy_ptz_controller.h>

template<class T>
class QnResourcePropertyAdaptor;

class NX_VMS_COMMON_API QnActivityPtzController:
    public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    enum Mode
    {
        Local, //< Fisheye mode
        Client, //< Client mode. Does not change resource properties
        Server //< Used at the server side (PTZ preset as an action, for example)
    };

    QnActivityPtzController(
        Mode mode,
        const QnPtzControllerPtr& baseController);
    virtual ~QnActivityPtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(
        const Vector& speed,
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

    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;

    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;

    virtual bool getData(
        QnPtzData* data,
        DataFields query,
        const Options& options) const override;

private:
    const Mode m_mode;
    QnResourcePropertyAdaptor<QnPtzObject>* m_adaptor;
};
