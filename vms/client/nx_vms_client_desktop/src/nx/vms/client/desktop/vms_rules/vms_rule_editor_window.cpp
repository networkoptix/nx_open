// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rule_editor_window.h"

#include "ui_vms_rule_editor_window.h"

#include <QtGui/QPalette>

#include <ui/common/palette.h>

#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/action_builder.h>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>

#include <nx/vms/client/desktop/vms_rules/dialog_action_event_editors/generic_event_params_widget.h>
#include <nx/vms/client/desktop/vms_rules/dialog_action_event_editors/http_action_params_widget.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

using namespace entity_item_model;

VmsRuleEditorWindow::VmsRuleEditorWindow(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::VmsRuleEditorWindow())
{
    m_ui->setupUi(this);

    m_ui->deleteRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/trash.png"), QPalette().color(QPalette::BrightText)));

    setPaletteColor(m_ui->panelsWidget, QPalette::Window, QPalette().color(QPalette::Base));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
VmsRuleEditorWindow::~VmsRuleEditorWindow()
{
}

void VmsRuleEditorWindow::setRule(nx::vms::rules::Rule* rule)
{
    // TODO: When actual data will be available create suitable event/action parameter editors
    // by factory.
    m_ui->genericEventParams->setFlatData(rule->eventFilters().front()->flatData());
    m_ui->httpActionParams->setFlatData(rule->actionBuilders().front()->flatData());
    m_ui->footerWidget->setComment(rule->comment());
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
