#ifndef QN_CAMERA_MANAGEMENT_WIDGET_H
#define QN_CAMERA_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>

namespace Ui {
    class CameraManagementWidget;
}

template<class T>
class QnResourcePropertyAdaptor;

class QnCameraManagementWidget: public QWidget {
    Q_OBJECT
public:
    QnCameraManagementWidget(QWidget *parent = NULL);
    virtual ~QnCameraManagementWidget();

    void updateFromSettings();
    void submitToSettings();

private slots:
    void at_autoDiscoveryCheckBox_clicked();

private:
    QScopedPointer<Ui::CameraManagementWidget> ui;
};

#endif // QN_CAMERA_MANAGEMENT_WIDGET_H
