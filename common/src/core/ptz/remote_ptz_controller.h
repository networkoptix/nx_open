#ifndef QN_REMOTE_PTZ_CONTROLLER_H
#define QN_REMOTE_PTZ_CONTROLLER_H

#include <QtCore/QUuid>

#include "abstract_ptz_controller.h"
#include "ptz_data.h"

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

    virtual void synchronize(Qn::PtzDataFields fields) override;

private slots:
    void at_continuousMove_replyReceived(int status, int handle);
    void at_absoluteMove_replyReceived(int status, int handle);
    void at_relativeMove_replyReceived(int status, int handle);

    void at_createPreset_replyReceived(int status, int handle);
    void at_updatePreset_replyReceived(int status, int handle);
    void at_removePreset_replyReceived(int status, int handle);
    void at_activatePreset_replyReceived(int status, int handle);

    void at_createTour_replyReceived(int status, int handle);
    void at_removeTour_replyReceived(int status, int handle);
    void at_activateTour_replyReceived(int status, int handle);

    void at_getData_replyReceived(int status, const QnPtzData &reply, int handle);

private:
    Q_SIGNAL void synchronizedLater(Qn::PtzDataFields fields);

private:
    QnNetworkResourcePtr m_resource;
    QnMediaServerResourcePtr m_server;
    QUuid m_sequenceId;
    int m_sequenceNumber;
    
    QHash<int, Qn::PtzDataFields> m_fieldsByHandle;
    QnPtzData m_data;
};


#endif // QN_REMOTE_PTZ_CONTROLLER_H
