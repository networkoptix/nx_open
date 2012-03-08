#ifndef QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
#define QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>

#include "api/AppServerConnection.h"

namespace Ui {
    class MultipleCameraSettingsWidget;
}

class QnMultipleCameraSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnMultipleCameraSettingsWidget(QWidget *parent);
    ~QnMultipleCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void updateFromResources();
    void submitToResources();

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget)

    QnVirtualCameraResourceList m_cameras;
    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;

    QString m_login;
    QString m_password;

    QnAppServerConnectionPtr m_connection;
};

#endif // QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
