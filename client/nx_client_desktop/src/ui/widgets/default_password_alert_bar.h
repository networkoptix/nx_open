#include <QtWidgets/QPushButton>

#include <ui/widgets/common/alert_bar.h>
#include <ui/workbench/workbench_context_aware.h>

#include <core/resource/resource_fwd.h>

class QnDefaultPasswordAlertBar: public QnAlertBar, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnWorkbenchContextAware;

public:
    QnDefaultPasswordAlertBar(QWidget* parent = nullptr);

    void setCameras(const QnVirtualCameraResourceList& cameras);

    QnVirtualCameraResourceList targetCameras() const;

private:
    bool setTargetCameras(const QnVirtualCameraResourceList& cameras);

    bool setMultipleSourceCameras(bool value);

    void updateState();

signals:
    void changeDefaultPasswordRequest();
    void targetCamerasChanged();

private:
    QPushButton* m_setPasswordButton = nullptr;
    QnVirtualCameraResourceList m_targetCameras;
    bool m_multipleSourceCameras;
};
