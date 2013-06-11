#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QWidget>
#include <QTimer>
#include <QIntValidator>

#include <api/model/kvpair.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/email.h>

namespace Ui {
    class QnSmtpSettingsWidget;
}

class QnPortNumberValidator: public QIntValidator {
    typedef QIntValidator base_type;

    Q_OBJECT
public:
    QnPortNumberValidator(QObject* parent = 0):
        base_type(parent) {}

    virtual QValidator::State validate(QString &input, int &pos) const override;

    virtual void fixup(QString &input) const override;
};

class QnSmtpSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    void update();
    void submit();

    void updateFocusedElement();

private:
    QnEmail::Settings settings();
    void stopTesting(QString result);
    void loadSettings(QString server, QnEmail::ConnectionType connectionType, int port = 0);

private slots:
    void at_portComboBox_currentIndexChanged(int index);
    void at_testButton_clicked();

    void at_cancelTestButton_clicked();
    void at_okTestButton_clicked();

    void at_timer_timeout();

    void at_settings_received(int status, const QnKvPairList &values, int handle);
    void at_settings_saved(int status, const QnKvPairList &values, int handle);

    void at_finishedTestEmailSettings(int status, bool result, int handle);

    void at_advancedCheckBox_toggled(bool toggled);
    void at_simpleEmail_textChanged(const QString &value);

private:
    QScopedPointer<Ui::QnSmtpSettingsWidget> ui;

    int m_requestHandle;
    int m_testHandle;

    QTimer* m_timeoutTimer;

    bool m_settingsReceived;
};

#endif // SMTP_SETTINGS_WIDGET_H
