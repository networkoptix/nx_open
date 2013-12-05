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
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

    virtual bool createPreset(QnPtzPreset *preset) override;
    virtual bool removePreset(const QnPtzPreset &preset) override;
    virtual bool activatePreset(const QnPtzPreset &preset) override;
    virtual bool getPresets(QnPtzPresetList *presets) override;

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
