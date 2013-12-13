#ifndef QN_REMOTE_PTZ_CONTROLLER_H
#define QN_REMOTE_PTZ_CONTROLLER_H

#include <QtCore/QUuid>
#include <QtCore/QAtomicInt>

#include "abstract_ptz_controller.h"

class QnRemotePtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnRemotePtzController(const QnNetworkResourcePtr &resource);
    virtual ~QnRemotePtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
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

    virtual bool synchronize(Qn::PtzDataFields query) override;

private slots:
    void at_replyReceived(int status, const QVariant &reply, int handle);

private:
    Q_SIGNAL void finishedLater(Qn::PtzCommand command, const QVariant &data);

    bool isPointless(Qn::PtzCommand command);

    int nextSequenceNumber();

private:
    QnNetworkResourcePtr m_resource;
    QnMediaServerResourcePtr m_server;
    QUuid m_sequenceId;
    QAtomicInt m_sequenceNumber;

    QMutex m_mutex;
    QHash<int, Qn::PtzCommand> m_commandByHandle;
    QnPtzData m_data;
};


#endif // QN_REMOTE_PTZ_CONTROLLER_H
