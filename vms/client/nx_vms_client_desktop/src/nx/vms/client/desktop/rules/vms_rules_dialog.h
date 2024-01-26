// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/rules/rules_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::rules {

class RulesTableModel;

class VmsRulesDialog: public QmlDialogWrapper, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit VmsRulesDialog(QWidget* parent);
    ~VmsRulesDialog() override;

    void setError(const QString& error);

    Q_INVOKABLE void addRule();
    Q_INVOKABLE void editSchedule(const QnUuidList& ids);
    Q_INVOKABLE void duplicateRule(QnUuid id);
    Q_INVOKABLE void editRule(QnUuid id);
    Q_INVOKABLE void deleteRules(const QnUuidList& ids);
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void openEventLogDialog();

private:
    QWidget* m_parentWidget{nullptr};
    RulesTableModel* m_rulesTableModel{nullptr};

    void deleteRulesImpl(const QnUuidList& ids);
    void saveRuleImpl(const std::shared_ptr<vms::rules::Rule>& rule);
    void resetToDefaultsImpl();
};

} // namespace nx::vms::client::desktop::rules
