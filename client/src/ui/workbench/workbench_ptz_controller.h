#ifndef QN_WORKBENCH_PTZ_CONTROLLER_H
#define QN_WORKBENCH_PTZ_CONTROLLER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QVector3D>

#include <utils/common/space_mapper.h>

#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"

class QnPtzSpaceMapper {
public:
    QnPtzSpaceMapper() {}
    QnPtzSpaceMapper(const QnVectorSpaceMapper &mapper): fromCamera(mapper), toCamera(mapper) {}
    QnPtzSpaceMapper(const QnVectorSpaceMapper &fromCamera, const QnVectorSpaceMapper &toCamera): fromCamera(fromCamera), toCamera(toCamera) {}

    QnVectorSpaceMapper fromCamera;
    QnVectorSpaceMapper toCamera;
};


class QnWorkbenchPtzController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPtzController(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzController();

    const QnPtzSpaceMapper *mapper(const QnVirtualCameraResourcePtr &camera) const;

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

private slots:
    void at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);
    
    void at_camera_statusChanged();
    void at_camera_statusChanged(const QnVirtualCameraResourcePtr &camera);

    void at_ptzGetPosition_replyReceived(int status, qreal xPos, qreal yPox, qreal zoomPos, int handle);
    void at_ptzSetPosition_replyReceived(int status, int handle);
    void at_ptzSetMovement_replyReceived(int status, int handle);

private:
    void sendGetPosition(const QnVirtualCameraResourcePtr &camera);
    void sendSetPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position);
    void sendSetMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement);

    void tryInitialize(const QnVirtualCameraResourcePtr &camera);

private:
    enum PtzRequestType {
        SetMovementRequest,
        SetPositionRequest,
        GetPositionRequest,
        RequestTypeCount
    };

    struct PtzData {
        PtzData();

        bool initialized;
        QVector3D position, physicalPosition;
        QVector3D movement;
        int attemptCount[RequestTypeCount];
    };

    QHash<QString, const QnPtzSpaceMapper *> m_mapperByModel;
    QHash<QnVirtualCameraResourcePtr, PtzData> m_dataByCamera;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByHandle;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByTimerId;
    QHash<int, QVector3D> m_requestByHandle;
};


#endif // QN_WORKBENCH_PTZ_CONTROLLER_H
