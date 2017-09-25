#ifndef QN_CACHING_PTZ_CONTROLLER_H
#define QN_CACHING_PTZ_CONTROLLER_H

#include <nx/utils/thread/mutex.h>

#include "proxy_ptz_controller.h"

class QnCachingPtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    QnCachingPtzController(const QnPtzControllerPtr& baseController);
    virtual ~QnCachingPtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

    virtual bool getPresets(QnPtzPresetList* presets) const override;
    virtual bool getTours(QnPtzTourList* tours) const override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;
    virtual bool getHomeObject(QnPtzObject* homeObject) const override;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData* data) const override;

protected:
    virtual void baseFinished(Qn::PtzCommand command, const QVariant& data) override;

private:
    bool initialize();

    template<class T>
    Qn::PtzDataFields updateCacheLocked(
        Qn::PtzDataField field,
        T QnPtzData::*member,
        const T& value);

    template<class T>
    Qn::PtzDataFields updateCacheLocked(
        Qn::PtzDataField field,
        T QnPtzData::*member,
        const QVariant& value);

    Qn::PtzDataFields updateCacheLocked(const QnPtzData& data);

private:
    bool m_initialized;
    mutable QnMutex m_mutex;
    QnPtzData m_data;
};


#endif // QN_CACHING_PTZ_CONTROLLER_H
