// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/email/email_fwd.h>

namespace Ui { class SmtpSettingsWidget; }

class QnSmtpSimpleSettingsWidget;
class QnSmtpAdvancedSettingsWidget;
class QnSmtpTestConnectionWidget;

class QnSmtpSettingsWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit QnSmtpSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnSmtpSettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    QnEmailSettings settings() const;
    void finishTesting();

    void atTestButtonClicked();
    void atAdvancedCheckBoxToggled(bool toggled);

private:
    QScopedPointer<Ui::SmtpSettingsWidget> ui;

    QnSmtpSimpleSettingsWidget* m_simpleSettingsWidget;
    QnSmtpAdvancedSettingsWidget* m_advancedSettingsWidget;
    QnSmtpTestConnectionWidget* m_testSettingsWidget;

    bool m_updating;
};
