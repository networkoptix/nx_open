// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_list_header_widget.h"

#include "ui_vms_rules_list_header_widget.h"

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/vms_rules/vms_rules_model_view/vms_rules_item_delegate.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

VmsRulesListHeaderWidget::VmsRulesListHeaderWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::VmsRulesListHeaderWidget())
{
    m_ui->setupUi(this);

    QFont boldFont;
    boldFont.setWeight(QFont::Bold);
    m_ui->eventHeaderLabel->setFont(boldFont);
    m_ui->actionHeaderLabel->setFont(boldFont);

    const auto brightTextColor = QPalette().color(QPalette::BrightText);
    setPaletteColor(m_ui->eventHeaderLabel, QPalette::WindowText, brightTextColor);
    setPaletteColor(m_ui->actionHeaderLabel, QPalette::WindowText, brightTextColor);

    setPaletteColor(m_ui->headerSeparatorLine, QPalette::Shadow, QPalette().color(QPalette::Mid));

    const auto leftMargin = style::Metrics::kStandardPadding;
    const auto rightMargin = VmsRulesItemDelegate::modificationMarkColumnWidth()
        - style::Metrics::kStandardPadding;
    m_ui->horizontalLayout->setContentsMargins(QMargins(leftMargin, 0, rightMargin, 0));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
VmsRulesListHeaderWidget::~VmsRulesListHeaderWidget()
{
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
