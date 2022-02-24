// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_action_params_widget.h"

#include "ui_http_action_params_widget.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/custom_style.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

HttpActionParamsWidget::HttpActionParamsWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::HttpActionParamsWidget())
{
    m_ui->setupUi(this);

    setupLineEditsPlaceholderColor();

    auto monoFont = monospaceFont();
    monoFont.setStretch((QFont::SemiCondensed + QFont::Unstretched) / 2); //< Little condensed, more looks cheap.
    m_ui->httpContentTextEdit->setFont(monoFont);
    m_ui->httpContentTextEdit->setWordWrapMode(QTextOption::NoWrap);
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
HttpActionParamsWidget::~HttpActionParamsWidget()
{
}

std::map<QString, QVariant> HttpActionParamsWidget::flatData() const
{
    NX_ASSERT(false, "Not implemented");
    return {};
}

void HttpActionParamsWidget::setFlatData(const std::map<QString, QVariant>& flatData)
{
    const auto urlItr = flatData.find("url/text");
    if (urlItr != std::cend(flatData))
        m_ui->httpUrlLineEdit->setText(urlItr->second.toString());

    const auto contentItr = flatData.find("content/text");
    if (contentItr != std::cend(flatData))
        m_ui->httpContentTextEdit->setPlainText(contentItr->second.toString());

    const auto loginItr = flatData.find("login/text");
    if (loginItr != std::cend(flatData))
        m_ui->loginLineEdit->setText(loginItr->second.toString());

    const auto passwordItr = flatData.find("password/text");
    if (passwordItr != std::cend(flatData))
        m_ui->passwordLineEdit->setText(passwordItr->second.toString());
}

void HttpActionParamsWidget::setReadOnly(bool value)
{
    m_ui->httpUrlLineEdit->setReadOnly(value);
    m_ui->httpContentTextEdit->setReadOnly(value);
    m_ui->contentTypeComboBox->setDisabled(value);
    m_ui->loginLineEdit->setReadOnly(value);
    m_ui->passwordLineEdit->setReadOnly(value);
    m_ui->authTypeComboBox->setDisabled(value);
    m_ui->requestTypeComboBox->setDisabled(value);
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
