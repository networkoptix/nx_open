#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <QtGui/QIntValidator>

#include <api/model/kvpair.h>
#include <nx_ec/ec_api.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/email.h>


namespace Ui {
    class SmtpSettingsWidget;
}

class QnSmtpSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    void updateFromSettings();
    void submitToSettings();

private:
    QnEmail::Settings settings();
    void stopTesting(QString result);
    void loadSettings(QString server, QnEmail::ConnectionType connectionType, int port = 0);
    void updateFocusedElement();

    void validateEmailSimple();
    void validateEmailAdvanced();
private slots:
    void at_portComboBox_currentIndexChanged(int index);
    void at_testButton_clicked();

    void at_cancelTestButton_clicked();
    void at_okTestButton_clicked();

    void at_timer_timeout();

    void at_finishedTestEmailSettings(int handle, ec2::ErrorCode errorCode);

    void at_advancedCheckBox_toggled(bool toggled);
private:
    QScopedPointer<Ui::SmtpSettingsWidget> ui;

    int m_testHandle;

    QTimer *m_timeoutTimer;
};

#endif // SMTP_SETTINGS_WIDGET_H
