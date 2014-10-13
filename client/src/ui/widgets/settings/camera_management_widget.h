#ifndef QN_CAMERA_MANAGEMENT_WIDGET_H
#define QN_CAMERA_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/connective.h>

namespace Ui {
    class CameraManagementWidget;
}

class QnCameraManagementWidget: public Connective<QnAbstractPreferencesWidget> {
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnCameraManagementWidget(QWidget *parent = NULL);
    virtual ~QnCameraManagementWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

private slots:
    void at_autoDiscoveryCheckBox_clicked();
    void updateFailoverWarning();

private:
    QScopedPointer<Ui::CameraManagementWidget> ui;
};

#endif // QN_CAMERA_MANAGEMENT_WIDGET_H
