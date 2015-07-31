#ifndef SMTP_SETTINGS_WIDGET_H
#define SMTP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <QtGui/QIntValidator>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/ldap_fwd.h>

namespace Ui {
    class LdapSettingsWidget;
}

struct QnTestLdapSettingsReply;

class QnLdapSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    explicit QnLdapSettingsWidget(QWidget *parent = 0);
    ~QnLdapSettingsWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

    virtual bool confirm() override;
    virtual bool discard() override;
private:
    QnLdapSettings settings() const;
    void stopTesting(const QString &result = QString());
    void finishTesting();
    void loadSettings(const QString &server, int port = 0);

    void validateEmailSimple();
    void validateEmailAdvanced();
private slots:
    void at_portComboBox_currentIndexChanged(int index);
    void at_testButton_clicked();

    void at_cancelTestButton_clicked();

    void at_timer_timeout();

    void at_testLdapSettingsFinished(int status, const QnTestLdapSettingsReply& reply, int handle);
private:
    QScopedPointer<Ui::LdapSettingsWidget> ui;

    int m_testHandle;
    bool m_updating;

    QTimer *m_timeoutTimer;
};

#endif // SMTP_SETTINGS_WIDGET_H
