#ifndef QN_PRESET_PTZ_CONTROLLER_H
#define QN_PRESET_PTZ_CONTROLLER_H

#include <QtCore/QMutex>

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;

struct QnPtzPresetRecord;
typedef QHash<QString, QnPtzPresetRecord> QnPtzPresetRecordHash;

class QnPresetPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnPresetPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnPresetPtzController();

    static bool extends(Qn::PtzCapabilities capabilities);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

private:
    QMutex m_mutex;
    QnResourcePropertyAdaptor<QnPtzPresetRecordHash> *m_adaptor;
    QAtomicInt m_changing;
};

#endif // QN_PRESET_PTZ_CONTROLLER_H
