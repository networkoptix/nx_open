#ifndef QN_TOUR_PTZ_CONTROLLER_H
#define QN_TOUR_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;
class QnPtzTourExecutor;

typedef QHash<QString, QnPtzTour> QnPtzTourHash;

class QnTourPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnTourPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnTourPtzController();

    static bool extends(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool createTour(const QnPtzTour &tour) override;
    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;
    virtual bool getTours(QnPtzTourList *tours) override;

private:
    bool createTourInternal(QnPtzTour tour);

private:
    QMutex m_mutex;
    QnResourcePropertyAdaptor<QnPtzTourHash> *m_adaptor;
    QnPtzTourExecutor *m_executor;
};


#endif // QN_TOUR_PTZ_CONTROLLER_H
