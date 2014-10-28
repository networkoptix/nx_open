#ifndef QN_NEWDW_PTZ_CONTROLLER_H
#define QN_NEWDW_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <QtCore/QScopedPointer>
#include <core/ptz/basic_ptz_controller.h>

class QnPlWatchDogResource;

class QnNewDWPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnNewDWPtzController(const QnPlWatchDogResourcePtr &resource);
    virtual ~QnNewDWPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;

    virtual bool getPresets(QnPtzPresetList *presets) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;
private:
    bool doQuery(const QString &request, QByteArray* body = 0) const;
    QString toInternalID(const QString& externalId);
    QString fromExtarnalID(const QString& externalId);
private:
    QSharedPointer<QnPlWatchDogResource> m_resource;
    QMap<QString, QString> m_extIdToIntId;
    mutable QMutex m_mutex;
    QMap<QString, QString> m_cachedData;
    QTime m_cacheUpdateTimer;
};

#endif // ENABLE_ONVIF
#endif // QN_NEWDW_PTZ_CONTROLLER_H
