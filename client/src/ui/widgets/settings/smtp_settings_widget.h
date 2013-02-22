#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QWidget>
#include <QTimer>

#include <api/model/kvpair.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnSmtpSettingsWidget;
}

class QDnsLookup;

class QnSmtpSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    void update();
    void submit();
private slots:
    void at_portComboBox_currentIndexChanged(int index);
    void at_testButton_clicked();
    void at_testResultButton_clicked();
    void at_timer_timeout();

    void at_settings_received(int status, const QByteArray& errorString, const QnKvPairList& settings, int handle);

    void at_finishedTestEmailSettings(int status, const QByteArray& errorString, bool result, int handle);

    void at_advancedCheckBox_toggled(bool toggled);
    void at_simpleEmail_textChanged(const QString &value);
private:
    QnKvPairList settings();
    void updateMailServers();
private:
    QScopedPointer<Ui::QnSmtpSettingsWidget> ui;

    int m_requestHandle;
    int m_testHandle;
    QDnsLookup* m_dns;

    QString m_autoMailServer;
    QTimer* m_timeoutTimer;

    bool m_settingsReceived;
};

#endif // SMTP_SETTINGS_WIDGET_H
