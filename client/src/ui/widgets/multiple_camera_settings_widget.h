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
    explicit QnMultipleCameraSettingsWidget(QWidget *parent, QnVirtualCameraResourceList cameras);
    ~QnMultipleCameraSettingsWidget();

public slots:
    virtual void accept();
    virtual void reject();
    virtual void buttonClicked(QAbstractButton *button);

    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    void updateView();
    void saveToModel();

    void save();

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget)

    QnVirtualCameraResourceList m_cameras;
    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;

    QString m_login;
    QString m_password;

    QnAppServerConnectionPtr m_connection;
};

#endif // QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
