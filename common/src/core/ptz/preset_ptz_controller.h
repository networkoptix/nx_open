#ifndef QN_PRESET_PTZ_CONTROLLER_H
#define QN_PRESET_PTZ_CONTROLLER_H

#include <nx/utils/thread/mutex.h>
#include <core/resource/resource_fwd.h>
#include <api/resource_property_adaptor.h>

#include "proxy_ptz_controller.h"


struct QnPtzPresetRecord;

typedef QHash<QString, QnPtzPresetRecord> QnPtzPresetRecordHash;

class QnPresetPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;
	typedef std::function<bool(QnPtzPresetRecordHash& records, QnPtzPreset)> PresetsActionFunc;

public:
    QnPresetPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnPresetPtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() override;

    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

private:
	QString serializePresets(const QnPtzPresetRecordHash& presets);
	QnPtzPresetRecordHash deserializePresets(const QString& presetsSerialized);
	bool doPresetsAction(PresetsActionFunc actionFunc, QnPtzPreset preset);

private:
    QnMutex m_mutex;
	QnVirtualCameraResourcePtr m_camera;
	std::unique_ptr<QnJsonResourcePropertyHandler<QnPtzPresetRecordHash>> m_propertyHandler;
};

#endif // QN_PRESET_PTZ_CONTROLLER_H
