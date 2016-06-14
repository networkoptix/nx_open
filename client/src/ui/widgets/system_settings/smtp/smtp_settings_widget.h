#pragma once

#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/email/email_fwd.h>

namespace Ui {
    class SmtpSettingsWidget;
}

class QnSmtpSimpleSettingsWidget;
class QnSmtpAdvancedSettingsWidget;
class QnSmtpTestConnectionWidget;

class QnSmtpSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    explicit QnSmtpSettingsWidget(QWidget *parent = 0);
    ~QnSmtpSettingsWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    QnEmailSettings settings() const;
    void finishTesting();

    void at_testButton_clicked();
    void at_advancedCheckBox_toggled(bool toggled);

private:
    QScopedPointer<Ui::SmtpSettingsWidget> ui;

    QnSmtpSimpleSettingsWidget* m_simpleSettingsWidget;
    QnSmtpAdvancedSettingsWidget* m_advancedSettingsWidget;
    QnSmtpTestConnectionWidget* m_testSettingsWidget;

    bool m_updating;
};
