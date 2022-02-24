// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_event_params_widget.h"

#include "ui_generic_event_params_widget.h"

#include <nx/utils/log/assert.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

GenericEventParamsWidget::GenericEventParamsWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::GenericEventParamsWidget())
{
    m_ui->setupUi(this);

    setupLineEditsPlaceholderColor();
    setPaletteColor(m_ui->hintLabel, QPalette::WindowText, QPalette().color(QPalette::Light));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
GenericEventParamsWidget::~GenericEventParamsWidget()
{
}

std::map<QString, QVariant> GenericEventParamsWidget::flatData() const
{
    NX_ASSERT(false, "Not implemented");
    return {};
}

void GenericEventParamsWidget::setFlatData(const std::map<QString, QVariant>& flatData)
{
    const auto captionItr = flatData.find("caption/string");
    if (captionItr != std::cend(flatData))
        m_ui->captionLineEdit->setText(captionItr->second.toString());
}

void GenericEventParamsWidget::setReadOnly(bool value)
{
    m_ui->captionLineEdit->setReadOnly(value);
    m_ui->descriptionLineEdit->setReadOnly(value);
    m_ui->sourceLineEdit->setReadOnly(value);
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
