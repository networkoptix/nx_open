#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <QtGui/QIntValidator>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/email.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>
#include "api/model/test_email_settings_reply.h"

namespace Ui {
    class SmtpSettingsWidget;
}

class QnSmtpSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

protected:
    virtual void showEvent( QShowEvent* event ) override;

private:
    QnEmail::Settings settings() const;
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

    void at_advancedCheckBox_toggled(bool toggled);
    void at_testEmailSettingsFinished(int status, const QnTestEmailSettingsReply& reply, int handle);
private:
    QScopedPointer<Ui::SmtpSettingsWidget> ui;

    int m_testHandle;

    QTimer *m_timeoutTimer;
};

#endif // SMTP_SETTINGS_WIDGET_H
