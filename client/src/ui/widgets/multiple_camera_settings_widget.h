#ifndef QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
#define QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"

namespace Ui {
    class MultipleCameraSettingsWidget;
}

class QnMultipleCameraSettingsWidget : public QWidget {
    Q_OBJECT;

public:
    explicit QnMultipleCameraSettingsWidget(QWidget *parent);
    ~QnMultipleCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    void updateFromResources();
    void submitToResources();

    bool hasChanges() const {
        return m_hasChanges;
    }

signals:
    void hasChangesChanged();

private slots:
    void at_dataChanged();

private:
    void setHasChanges(bool hasChanges);

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget);

    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;
    QnVirtualCameraResourceList m_cameras;
    bool m_hasChanges;
};

#endif // QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
