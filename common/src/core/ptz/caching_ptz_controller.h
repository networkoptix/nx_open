#ifndef QN_CACHING_PTZ_CONTROLLER_H
#define QN_CACHING_PTZ_CONTROLLER_H

#include <utils/common/mutex.h>

#include "proxy_ptz_controller.h"

class QnCachingPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnCachingPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnCachingPtzController();

    static bool extends(Qn::PtzCapabilities capabilities);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

    virtual bool createTour(const QnPtzTour &tour) override;
    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;
    virtual bool getTours(QnPtzTourList *tours) override;

    virtual bool getActiveObject(QnPtzObject *activeObject) override;
    virtual bool updateHomeObject(const QnPtzObject &homeObject) override;
    virtual bool getHomeObject(QnPtzObject *homeObject) override;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits) override;
    virtual bool runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data) override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data) override;

protected:
    virtual void baseFinished(Qn::PtzCommand command, const QVariant &data) override;

private:
    bool initialize();
    
    template<class T>
    Qn::PtzDataFields updateCacheLocked(Qn::PtzDataField field, T QnPtzData::*member, const T &value);
    template<class T>
    Qn::PtzDataFields updateCacheLocked(Qn::PtzDataField field, T QnPtzData::*member, const QVariant &value);
    Qn::PtzDataFields updateCacheLocked(const QnPtzData &data);

private:
    bool m_initialized;
    QnMutex m_mutex;
    QnPtzData m_data;
};


#endif // QN_CACHING_PTZ_CONTROLLER_H
