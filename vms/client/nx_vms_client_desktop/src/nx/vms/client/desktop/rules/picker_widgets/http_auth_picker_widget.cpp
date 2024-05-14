// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_auth_picker_widget.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/workbench/workbench_context.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

struct HttpAuthPicker::Private
{
    QComboBox* comboBox{nullptr};
    QLineEdit* login{nullptr};
    QLineEdit* password{nullptr};
    QLineEdit* token{nullptr};
    QVBoxLayout* mainLayout{nullptr};

    QWidget* loginPasswordGroup{nullptr};
    QWidget* tokenGroup{nullptr};

    const QString bearerStr = "Bearer";
    const QString basicStr = "Basic";
    const QString digestStr = "Digest";

    QHBoxLayout* createWidgetWithHint(const QString& labelText, QWidget* widget)
    {
        auto lineLayout = new QHBoxLayout();
        lineLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
        lineLayout->setContentsMargins(
            style::Metrics::kDefaultTopLevelMargin, 4, style::Metrics::kDefaultTopLevelMargin, 4);

        auto label = new WidgetWithHint<QnElidedLabel>;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setElideMode(Qt::ElideRight);
        label->setFixedHeight(28);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        label->setText(labelText);
        lineLayout->addWidget(label);

        lineLayout->addWidget(widget);
        lineLayout->setAlignment(label, Qt::AlignTop);
        lineLayout->setStretch(0, 1);
        lineLayout->setStretch(1, 5);

        widget->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        return lineLayout;
    }

    QLineEdit* createLineEdit(bool isPassword)
    {
        auto lineEdit = new QLineEdit;
        lineEdit->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        if (isPassword)
            lineEdit->setEchoMode(QLineEdit::Password);
        return lineEdit;
    }
};

HttpAuthPicker::HttpAuthPicker(
    vms::rules::HttpAuthField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    base(field, context, parent),
    d(new HttpAuthPicker::Private())
{
    auto mainLayout = new QVBoxLayout{this};
    d->comboBox = new QComboBox;
    mainLayout->addLayout(d->createWidgetWithHint(tr("Authentication"), d->comboBox));

    d->loginPasswordGroup = new QWidget(this);
    auto loginPasswordLayout = new QVBoxLayout(d->loginPasswordGroup);
    d->login = d->createLineEdit(false);
    loginPasswordLayout->addLayout(d->createWidgetWithHint(tr("Login"), d->login));

    d->password = d->createLineEdit(true);
    loginPasswordLayout->addLayout(d->createWidgetWithHint(tr("Password"), d->password));

    mainLayout->addWidget(d->loginPasswordGroup);

    d->token = d->createLineEdit(true);
    d->tokenGroup = new QWidget(this);
    d->tokenGroup->setLayout(d->createWidgetWithHint(tr("Token"), d->token));

    mainLayout->addWidget(d->tokenGroup);
    d->tokenGroup->setVisible(false);

    d->comboBox->addItem(DropdownTextPickerWidgetStrings::autoValue());
    d->comboBox->addItem(d->bearerStr);
    d->comboBox->addItem(d->digestStr);
    d->comboBox->addItem(d->basicStr);

    // All connectors, which can cause call of the `m_field`,
    // must be created after all object initializations to avoid segfault on uninitialized field.
    connect(d->comboBox,
        &QComboBox::currentIndexChanged,
        this,
        &HttpAuthPicker::onCurrentIndexChanged);
    connect(d->token, &QLineEdit::textEdited, this, &HttpAuthPicker::onTokenChanged);
    connect(d->password, &QLineEdit::textEdited, this, &HttpAuthPicker::onPasswordChanged);
    connect(d->login, &QLineEdit::textEdited, this, &HttpAuthPicker::onLoginChanged);
}

HttpAuthPicker::~HttpAuthPicker()
{
}

void HttpAuthPicker::onLoginChanged(const QString& text)
{
    m_field->setLogin(text.toStdString());
}

void HttpAuthPicker::onPasswordChanged(const QString& text)
{
    m_field->setPassword(text.toStdString());
}

void HttpAuthPicker::onTokenChanged(const QString& text)
{
    m_field->setToken(text.toStdString());
}

void HttpAuthPicker::updateUi()
{
    QSignalBlocker blocker{d->comboBox};
    QSignalBlocker blockerLogin{d->login};
    QSignalBlocker blockerPassword{d->password};
    QSignalBlocker blockerToken{d->token};

    const auto fieldValue = m_field->authType();
    switch (fieldValue)
    {
        case nx::network::http::AuthType::authBearer:
            d->comboBox->setCurrentText(d->bearerStr);
            break;
        case nx::network::http::AuthType::authBasic:
            d->comboBox->setCurrentText(d->basicStr);
            break;
        case nx::network::http::AuthType::authDigest:
            d->comboBox->setCurrentText(d->digestStr);
            break;
        default:
            d->comboBox->setCurrentText(DropdownTextPickerWidgetStrings::autoValue());
    }

    const bool isBearer = fieldValue == nx::network::http::AuthType::authBearer;
    d->loginPasswordGroup->setVisible(!isBearer);
    d->tokenGroup->setVisible(isBearer);

    d->login->setText(QString::fromStdString(m_field->login()));
    d->password->setText(QString::fromStdString(m_field->password()));
    d->token->setText(QString::fromStdString(m_field->token()));
}

void HttpAuthPicker::onCurrentIndexChanged(int /*index*/)
{
    const auto value = d->comboBox->currentText().trimmed();
    if (value == d->bearerStr)
        m_field->setAuthType(nx::network::http::AuthType::authBearer);
    else if (value == d->basicStr)
        m_field->setAuthType(nx::network::http::AuthType::authBasic);
    else //< Auto and digest type.
        m_field->setAuthType(nx::network::http::AuthType::authDigest);

    const bool isBearer = value == d->bearerStr;
    d->loginPasswordGroup->setVisible(!isBearer);
    d->tokenGroup->setVisible(isBearer);
}

void HttpAuthPicker::setReadOnly(bool value)
{
    setEnabled(!value);
}

} // namespace nx::vms::client::desktop::rules
