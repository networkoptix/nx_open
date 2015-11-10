#ifndef QN_TOUR_PTZ_CONTROLLER_H
#define QN_TOUR_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;
class QnTourPtzExecutor;

typedef QHash<QString, QnPtzTour> QnPtzTourHash;

class QnTourPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnTourPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnTourPtzController();

    static bool extends(Qn::PtzCapabilities capabilities);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;

    virtual bool createTour(const QnPtzTour &tour) override;
    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;
    virtual bool getTours(QnPtzTourList *tours) override;

private:
    void clearActiveTour();

private:
    QnMutex m_mutex;
    QnResourcePropertyAdaptor<QnPtzTourHash> *m_adaptor;
    QnPtzTour m_activeTour;
    QnTourPtzExecutor *m_executor;
};


#endif // QN_TOUR_PTZ_CONTROLLER_H
