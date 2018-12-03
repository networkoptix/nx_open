#ifndef QN_NEWDW_PTZ_CONTROLLER_H
#define QN_NEWDW_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <QtCore/QScopedPointer>
#include <core/ptz/basic_ptz_controller.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnPlWatchDogResource;

class QnNewDWPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnNewDWPtzController(const QnDigitalWatchdogResourcePtr &resource);
    virtual ~QnNewDWPtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;
    virtual bool continuousMove(
        const nx::core::ptz::Vector& speedVector,
        const nx::core::ptz::Options& options) override;

    virtual bool getPresets(QnPtzPresetList *presets) const override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;

private:
    bool doQuery(const QString &request, QByteArray* body = 0) const;
    QString toInternalID(const QString& externalId);
    QString fromExtarnalID(const QString& externalId);
private:
    QnDigitalWatchdogResourcePtr m_resource;
    QMap<QString, QString> m_extIdToIntId;
    mutable QnMutex m_mutex;
    mutable QMap<QString, QString> m_cachedData;
    QTime m_cacheUpdateTimer;
};

#endif // ENABLE_ONVIF
#endif // QN_NEWDW_PTZ_CONTROLLER_H
