#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QWidget>

#include <api/model/kvpair.h>

namespace Ui {
    class QnSmtpSettingsWidget;
}

class QDnsLookup;

class QnSmtpSettingsWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    void update();
    void submit();
private slots:
    void at_portComboBox_currentIndexChanged(int index);

    void at_settings_received(int status, const QByteArray& errorString, const QnKvPairList& settings, int handle);
    void at_mailServers_received();

    void at_testButton_clicked();
    void at_autoDetectCheckBox_toggled(bool toggled);

    void handleServers();
private:
    void updateOverlay();

private:
    QScopedPointer<Ui::QnSmtpSettingsWidget> ui;

    int m_requestHandle;
    QDnsLookup* m_dns;

    QString m_autoMailServer;
    QString m_autoUser;

    bool m_settingsReceived;
    bool m_mailServersReceived;
};

#endif // SMTP_SETTINGS_WIDGET_H
