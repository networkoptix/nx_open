#ifndef QN_WORKBENCH_PTZ_CONTROLLER_H
#define QN_WORKBENCH_PTZ_CONTROLLER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QVector3D>

#include <utils/math/space_mapper.h>

#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"

class QnWorkbenchPtzMapperWatcher;

class QnWorkbenchPtzController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzController(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzController();

    /**
     * \param camera                    Camera to get PTZ position for.
     * \returns                         Current PTZ position for the given camera,
     *                                  or NaN if it is not available.
     *                                  Use <tt>qIsNaN</tt> to check for NaN.
     */
    QVector3D position(const QnVirtualCameraResourcePtr &camera) const;

    QVector3D physicalPosition(const QnVirtualCameraResourcePtr &camera) const;

    /**
     * \param camera                    Camera to set PTZ position for.
     * \param position                  New PTZ position for the given camera.
     */
    void setPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position);

    void setPhysicalPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &physicalPosition);

    // TODO: #Elric remove?
    void updatePosition(const QnVirtualCameraResourcePtr &camera);

    /**
     * \param camera                    Camera to get current PTZ continuous movement speed for.
     * \returns                         Current PTZ continuous movement speed for the given
     *                                  camera, or NaN if it is not available.
     *                                  Use <tt>qIsNaN</tt> to check for NaN.
     */
    QVector3D movement(const QnVirtualCameraResourcePtr &camera) const;

    /**
     * \param camera                    Camera to set current PTZ continuous movement speed for.
     * \param movement                  New PTZ continuous movement speed for the given camera.
     */
    void setMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement);

signals:
    void movementChanged(const QnVirtualCameraResourcePtr &camera);
    void positionChanged(const QnVirtualCameraResourcePtr &camera);

private slots:
    void at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);
    
    void at_resource_statusChanged(const QnResourcePtr &resource);

    void at_ptzGetPosition_replyReceived(int status, const QVector3D &position, int handle);
    void at_ptzSetPosition_replyReceived(int status, int handle);
    void at_ptzSetMovement_replyReceived(int status, int handle);

private:
    void sendGetPosition(const QnVirtualCameraResourcePtr &camera);
    void sendSetPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position);
    void sendSetMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement);

    void tryInitialize(const QnVirtualCameraResourcePtr &camera);

    void emitChanged(const QnVirtualCameraResourcePtr &camera, bool position, bool movement);

private:
    enum PtzRequestType {
        SetMovementRequest,
        SetPositionRequest,
        GetPositionRequest,
        RequestTypeCount
    };

    struct PtzData {
        PtzData();

        QUuid sequenceId;
        int sequenceNumber;
        bool initialized;
        QVector3D position, physicalPosition;
        QVector3D movement;
        int attemptCount[RequestTypeCount];
    };

    QnWorkbenchPtzMapperWatcher *m_mapperWatcher;
    QHash<QnVirtualCameraResourcePtr, PtzData> m_dataByCamera;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByHandle;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByTimerId;
    QHash<int, QVector3D> m_requestByHandle;
};


#endif // QN_WORKBENCH_PTZ_CONTROLLER_H
