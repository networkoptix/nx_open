// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_dialog.h"

#include "ui_vms_rules_dialog.h"

#include "vms_rule_editor_window.h"

#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/vms_rules/dialog_action_event_editors/generic_event_params_widget.h>
#include <nx/vms/client/desktop/vms_rules/dialog_action_event_editors/http_action_params_widget.h>
#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_filter_model.h>
#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_item_delegate.h>
#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_list_model.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/event_filter.h>
#include <ui/common/indents.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

using namespace entity_item_model;

VmsRulesDialog::VmsRulesDialog(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::VmsRulesDialog()),
    m_ruleEditorWindow(new VmsRuleEditorWindow(parent)),
    m_rulesListModel(new EntityItemModel()),
    m_rulesFilterModel(new VmsRulesFilterModel())
{
    m_ui->setupUi(this);

    const auto buttonIconColor = QPalette().color(QPalette::BrightText);

    m_ui->newRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/arythmetic_plus.png"), buttonIconColor));
    m_ui->deleteRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/trash.png"), buttonIconColor));

    const auto midlightColor = QPalette().color(QPalette::Midlight);
    setPaletteColor(m_ui->searchLineEdit, QPalette::PlaceholderText, midlightColor);

    m_ruleSet = nx::vms::rules::Engine::instance()->cloneRules();

    setupRuleListView();
}

VmsRulesDialog::~VmsRulesDialog()
{
    m_ruleEditorWindow->deleteLater();
}

void VmsRulesDialog::setupRuleListView()
{
    m_ui->ruleListView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(
        QnIndents(style::Metrics::kStandardPadding, style::Metrics::kStandardPadding)));

    auto scrollBar = new SnappedScrollBar(m_ui->ruleListHolder);
    m_ui->ruleListView->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseItemViewPaddingWhenVisible(true);

    m_rulesListEntity = makeKeyList<QnUuid>(vmsRuleItemCreator(&m_ruleSet), noOrder());
    for (const auto& rule: m_ruleSet)
        m_rulesListEntity->addItem(rule.first);

    m_rulesListModel->setRootEntity(m_rulesListEntity.get());
    m_rulesFilterModel->setSourceModel(m_rulesListModel.get());

    m_ui->ruleListView->setModel(m_rulesFilterModel.get());
    m_ui->ruleListView->setItemDelegate(new VmsRulesItemDelegate(m_ui->ruleListView));

    const auto selectionModel = m_ui->ruleListView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& current)
        {
            if (!current.isValid())
            {
                m_ruleEditorWindow->hide();
                return;
            }
            const auto ruleId = current.data(VmsRulesModelRoles::ruleIdRole).value<QnUuid>();
            m_ruleEditorWindow->setRule(m_ruleSet.at(ruleId).get());
            m_ruleEditorWindow->show();
        });

    m_rulesFilterModel->setDynamicSortFilter(true);
    connect(m_ui->searchLineEdit, &SearchLineEdit::textChanged,
        m_rulesFilterModel.get(), &VmsRulesFilterModel::setFilterString);
}

void VmsRulesDialog::showEvent(QShowEvent* event)
{
    const auto selectionModel = m_ui->ruleListView->selectionModel();
    if (!selectionModel)
        return;

    const auto selectedIndexes = selectionModel->selectedIndexes();
    if (selectedIndexes.isEmpty())
        return;

    const auto ruleId =
        selectedIndexes.first().data(VmsRulesModelRoles::ruleIdRole).value<QnUuid>();
    m_ruleEditorWindow->setRule(m_ruleSet.at(ruleId).get());
    m_ruleEditorWindow->show();
}

void VmsRulesDialog::hideEvent(QHideEvent* event)
{
    m_ruleEditorWindow->hide();
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
