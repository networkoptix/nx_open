#ifndef QN_PRESET_PTZ_CONTROLLER_H
#define QN_PRESET_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnStringKvPairUsageHelper;

class QnPresetPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnPresetPtzController(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool createPreset(QnPtzPreset *preset) override;
    virtual bool removePreset(const QnPtzPreset &preset) override;
    virtual bool activatePreset(const QnPtzPreset &preset) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

private:
    QnStringKvPairUsageHelper *m_helper;
};

#endif // QN_PRESET_PTZ_CONTROLLER_H
