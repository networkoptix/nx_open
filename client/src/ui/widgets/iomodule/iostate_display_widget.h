#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H

#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>

#include "api/model/api_ioport_data.h"
#include <ui/dialogs/dialog.h>
#include <ui/workbench/workbench_context_aware.h>
#include "ui_iostate_display_widget.h"

#include <utils/common/connective.h>

class QnIOModuleMonitor;
typedef QSharedPointer<QnIOModuleMonitor> QnIOModuleMonitorPtr;

class QnIOStateDisplayWidget: public Connective<QnDialog>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnDialog> base_type;

public: 
    QnIOStateDisplayWidget(QWidget *parent = NULL);
    void setCamera(const QnVirtualCameraResourcePtr& camera);
signals:
private slots:
    void at_connectionOpened();
    void at_connectionClosed();
    void at_cameraPropertyChanged(const QnResourcePtr & /*res*/, const QString & key);
    void at_ioStateChanged(const QnIOStateData& value);
    void openConnection();
    void at_buttonClicked();
    void at_buttonStateChanged(bool toggled);
    void at_cameraStatusChanged(const QnResourcePtr & res);
    void at_timer();
private:
    void updateControls();
private:
    QScopedPointer<Ui::IOStateDisplayWidget> ui;

    QnVirtualCameraResourcePtr m_camera;
    QnIOModuleMonitorPtr m_monitor;
    
    struct ModelData
    {
        ModelData(): widget(0) { btnPressTime.invalidate(); }

        QnIOPortData ioConfigData; // port configurating parameters: name e.t.c
        QnIOStateData ioState;     // current port state on or off
        QWidget* widget;           // associated widget
        QElapsedTimer btnPressTime; // button press timeout
    };

    QMap<QString, ModelData> m_model;
    QTimer m_timer;
};


#endif // QN_CALENDAR_WIDGET_H
