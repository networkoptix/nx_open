#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/workbench/workbench_context_aware.h>

class QAbstractButton;

class QnCameraSettingsWidget;
class QnWorkbenchContext;

class QnCameraSettingsDialog: public QDialog, protected QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnCameraSettingsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags =  
#ifdef Q_OS_MAC
        Qt::Tool
#else
        0
#endif
        );
    virtual ~QnCameraSettingsDialog();

    QnCameraSettingsWidget *widget() const {
        return m_settingsWidget;
    }

    void ignoreAcceptOnce() {
        m_ignoreAccept = true;
    }

signals:
    void buttonClicked(QDialogButtonBox::StandardButton button);
    void advancedSettingChanged();
    void scheduleExported(const QnVirtualCameraResourceList &cameras);
    void cameraOpenRequested();

private slots:
    void at_buttonBox_clicked(QAbstractButton *button);
    void at_settingsWidget_hasChangesChanged();
    void at_settingsWidget_modeChanged();
    void at_advancedSettingChanged();

    void at_diagnoseButton_clicked();
    void at_rulesButton_clicked();

    void acceptIfSafe();

private:
    QPointer<QnWorkbenchContext> m_context;
    QnCameraSettingsWidget *m_settingsWidget;
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_applyButton, *m_okButton, *m_openButton, *m_diagnoseButton, *m_rulesButton;
    bool m_ignoreAccept;
};


#endif // QN_CAMERA_SETTINGS_DIALOG_H
