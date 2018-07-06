#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class RecordingStatusHelper: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnUuid cameraId READ cameraId WRITE setCameraId NOTIFY cameraIdChanged)
    Q_PROPERTY(int recordingMode READ recordingMode NOTIFY recordingModeChanged)
    Q_PROPERTY(QString tooltip READ tooltip NOTIFY recordingModeChanged)
    Q_PROPERTY(QString shortTooltip READ shortTooltip NOTIFY recordingModeChanged)
    Q_PROPERTY(QString qmlIconName READ qmlIconName NOTIFY recordingModeChanged)

public:
    RecordingStatusHelper(QObject* parent = nullptr);

    QnUuid cameraId() const;
    void setCameraId(const QnUuid& id);

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    int recordingMode() const;
    QString tooltip() const;
    QString shortTooltip() const;
    QString qmlIconName() const;
    QIcon icon() const;

    static int currentRecordingMode(const QnVirtualCameraResourcePtr& camera);
    static QString tooltip(int recordingMode);
    static QString shortTooltip(int recordingMode);
    static QString qmlIconName(int recordingMode);
    static QIcon icon(int recordingMode);

    static void registerQmlType();

signals:
    void cameraIdChanged();
    void recordingModeChanged();

private:
    void updateRecordingMode();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    int m_recordingMode = 0;
    int m_updateTimerId = -1;
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace desktop
} // namespace client
} // namespace nx
