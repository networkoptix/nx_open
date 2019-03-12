#pragma once

#include <core/ptz/proxy_ptz_controller.h>
#include <common/common_module_aware.h>

template<class T>
class QnResourcePropertyAdaptor;

class QnActivityPtzController: public QnProxyPtzController, public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    enum Mode
    {
        Local, //< Fisheye mode
        Client, //< Client mode. Does not change resource properties
        Server //< Used at the server side (PTZ preset as an action, for example)
    };

    QnActivityPtzController(QnCommonModule* commonModule, Mode mode, const QnPtzControllerPtr &baseController);
    virtual ~QnActivityPtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space
        , const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;
    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;

    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;

    virtual bool getData(
        Qn::PtzDataFields query, QnPtzData* data,
        const nx::core::ptz::Options& options) const override;

private:
    const Mode m_mode;
    QnResourcePropertyAdaptor<QnPtzObject>* m_adaptor;
};
