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
private:
    bool doQuery(const QString &request) const;
private:
    QSharedPointer<QnPlWatchDogResource> m_resource;
};

#endif // ENABLE_ONVIF
#endif // QN_NEWDW_PTZ_CONTROLLER_H
