#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include "api/AppServerConnection.h"

#include "camera_settings_tab.h"

namespace Ui {
    class SingleCameraSettingsWidget;
}

class QnCameraMotionMaskWidget;

class QnSingleCameraSettingsWidget : public QDialog
{
    Q_OBJECT

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    bool hasUnsubmittedData() const {
        return m_hasUnsubmittedData;
    }


public slots:
    virtual void accept() override;
    virtual void reject() override;

private:
    void updateData();
    void submitData();
    void saveData();

private slots:
    void at_requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);
    void at_tabWidget_currentChanged();
    void at_dataChanged();

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget)

    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnAppServerConnectionPtr m_connection;
    bool m_hasUnsubmittedData;

    QnCameraMotionMaskWidget *m_motionWidget;
};

#endif // CAMERA_SETTINGS_DIALOG_H
