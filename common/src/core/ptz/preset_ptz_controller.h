#ifndef QN_PRESET_PTZ_CONTROLLER_H
#define QN_PRESET_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnStringKvPairUsageHelper;
class QnPtzPresetRecordList;
class QnPresetPtzControllerPrivate;

class QnPresetPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnPresetPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnPresetPtzController();

    static bool extends(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool createPreset(QnPtzPreset *preset) override;
    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

private:
    QScopedPointer<QnPresetPtzControllerPrivate> d;
};

#endif // QN_PRESET_PTZ_CONTROLLER_H
