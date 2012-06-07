#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

class QAbstractButton;

class QnCameraSettingsWidget;
class QnWorkbenchContext;

class QnCameraSettingsDialog: public QDialog {
    Q_OBJECT;
public:
    QnCameraSettingsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnCameraSettingsDialog();

    QnCameraSettingsWidget *widget() const {
        return m_settingsWidget;
    }

signals:
    void buttonClicked(QDialogButtonBox::StandardButton button);

private slots:
    void at_buttonBox_clicked(QAbstractButton *button);
    void at_settingsWidget_hasChangesChanged();
    void at_settingsWidget_modeChanged();

private:
    QWeakPointer<QnWorkbenchContext> m_context;
    QnCameraSettingsWidget *m_settingsWidget;
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_applyButton, *m_okButton;
};


#endif // QN_CAMERA_SETTINGS_DIALOG_H
