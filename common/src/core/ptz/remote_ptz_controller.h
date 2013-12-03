#ifndef QN_REMOTE_PTZ_CONTROLLER_H
#define QN_REMOTE_PTZ_CONTROLLER_H

#include <QtCore/QUuid>

#include "abstract_ptz_controller.h"

class QnRemotePtzController: public QnAbstractPtzController {
    Q_OBJECT
public:
    QnRemotePtzController(const QnNetworkResourcePtr &resource);
    virtual ~QnRemotePtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(const QVector3D &position) override;
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override;

    virtual bool getPosition(QVector3D *position) override;
    virtual bool getLimits(QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

private slots:
    void at_continuousMove_replyReceived(int status, int handle);
    void at_absoluteMove_replyReceived(int status, int handle);
    void at_relativeMove_replyReceived(int status, int handle);

private:
    QnNetworkResourcePtr m_resource;
    QnMediaServerResourcePtr m_server;
    QUuid m_sequenceId;
    int m_sequenceNumber;
};


#endif // QN_REMOTE_PTZ_CONTROLLER_H
