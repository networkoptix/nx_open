// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_auth_picker_widget.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/widgets/common/elided_label.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/strings.h"

namespace nx::vms::client::desktop::rules {

namespace {

template<class T>
T* createLayout(QWidget* parent = nullptr)
{
    auto layout = new T{parent};
    layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

    return layout;
}

void adjustLayout(QHBoxLayout* layout)
{
    layout->setStretch(0, 1);
    layout->setStretch(1, 5);
}

} // namespace

struct HttpAuthPicker::Private
{
    QComboBox* comboBox{nullptr};
    QLineEdit* login{nullptr};
    QLineEdit* password{nullptr};
    QLineEdit* token{nullptr};
    AlertLabel* alertLabel{nullptr};
    QVBoxLayout* mainLayout{nullptr};

    QWidget* loginPasswordGroup{nullptr};
    QWidget* tokenGroup{nullptr};
    QWidget* alertGroup{nullptr};

    const QString bearerStr = "Bearer";
    const QString basicStr = "Basic";
    const QString digestStr = "Digest";

    QHBoxLayout* createWidgetWithHint(const QString& labelText, QWidget* widget)
    {
        auto lineLayout = createLayout<QHBoxLayout>();

        auto label = new WidgetWithHint<QnElidedLabel>;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setElideMode(Qt::ElideRight);
        label->setFixedHeight(28);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        label->setText(labelText);
        lineLayout->addWidget(label);

        lineLayout->addWidget(widget);
        lineLayout->setAlignment(label, Qt::AlignTop);
        adjustLayout(lineLayout);

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
    auto mainLayout = createLayout<QVBoxLayout>(this);
    mainLayout->setContentsMargins(
            style::Metrics::kDefaultTopLevelMargin, 4, style::Metrics::kDefaultTopLevelMargin, 4);
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

    d->alertGroup = new QWidget{this};
    auto alertLayout = createLayout<QHBoxLayout>();
    d->alertGroup->setLayout(alertLayout);
    d->alertLabel = new AlertLabel{d->alertGroup};
    alertLayout->addWidget(new QWidget);
    alertLayout->addWidget(d->alertLabel);
    adjustLayout(alertLayout);

    mainLayout->addWidget(d->alertGroup);
    d->alertGroup->setVisible(false);

    d->comboBox->addItem(Strings::autoValue(),
        QVariant::fromValue(nx::network::http::AuthType::authBasicAndDigest));
    d->comboBox->addItem(
        d->bearerStr, QVariant::fromValue(nx::network::http::AuthType::authBearer));
    d->comboBox->addItem(
        d->digestStr, QVariant::fromValue(nx::network::http::AuthType::authDigest));
    d->comboBox->addItem(d->basicStr, QVariant::fromValue(nx::network::http::AuthType::authBasic));

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
    m_field->setLogin(text);

    setEdited();
}

void HttpAuthPicker::onPasswordChanged(const QString& text)
{
    m_field->setPassword(text);

    setEdited();
}

void HttpAuthPicker::onTokenChanged(const QString& text)
{
    m_field->setToken(text);

    setEdited();
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
            d->comboBox->setCurrentText(Strings::autoValue());
    }

    const bool isBearer = fieldValue == nx::network::http::AuthType::authBearer;
    d->loginPasswordGroup->setVisible(!isBearer);
    d->tokenGroup->setVisible(isBearer);

    d->login->setText(m_field->login());
    d->password->setText(m_field->password());
    d->token->setText(m_field->token());

    if (isEdited())
        setValidity(fieldValidity());
}

void HttpAuthPicker::setValidity(const vms::rules::ValidationResult& validationResult)
{
    if (validationResult.validity == QValidator::State::Acceptable
        || validationResult.description.isEmpty())
    {
        resetErrorStyle(d->token);
        resetErrorStyle(d->login);
        resetErrorStyle(d->password);

        d->alertGroup->setVisible(false);
        d->alertLabel->setText({});
        return;
    }

    if (m_field->authType() == nx::network::http::AuthType::authBearer)
    {
        setErrorStyle(d->token);
    }
    else
    {
        if (m_field->login().isEmpty())
            setErrorStyle(d->login);
        else
            resetErrorStyle(d->login);

        if (m_field->password().isEmpty())
            setErrorStyle(d->password);
        else
            resetErrorStyle(d->password);
    }

    d->alertLabel->setType(validationResult.validity == QValidator::State::Intermediate
        ? AlertLabel::Type::warning
        : AlertLabel::Type::error);
    d->alertLabel->setText(validationResult.description);
    d->alertGroup->setVisible(true);
}

void HttpAuthPicker::onCurrentIndexChanged(int /*index*/)
{
    const auto value = d->comboBox->currentData().value<nx::network::http::AuthType>();
    m_field->setAuthType(value);

    setEdited();

    const bool isBearer = value == nx::network::http::AuthType::authBearer;
    d->loginPasswordGroup->setVisible(!isBearer);
    d->tokenGroup->setVisible(isBearer);
}

void HttpAuthPicker::setReadOnly(bool value)
{
    setEnabled(!value);
}

bool HttpAuthPicker::hasSeparator()
{
    return true;
}

} // namespace nx::vms::client::desktop::rules
