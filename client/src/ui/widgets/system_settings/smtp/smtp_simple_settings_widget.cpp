#include "smtp_simple_settings_widget.h"
#include "ui_smtp_simple_settings_widget.h"

#include <ui/common/mandatory.h>
#include <ui/common/read_only.h>
#include <ui/style/custom_style.h>

#include <utils/common/app_info.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/email/email.h>

QnEmailSettings QnSimpleSmtpSettings::toSettings( const QnEmailSettings &base ) const {
    QnEmailSettings result;
    QnEmailAddress address(email);
    if (address.smtpServer().isNull()) {
        /* Current simple settings are not valid. */
        result = base;
    } else {
        result = address.settings();
        result.user = email;
    }
    result.email = email;
    result.password = password;
    result.signature = signature;
    result.supportEmail = supportEmail;
    return result;
}

QnSimpleSmtpSettings QnSimpleSmtpSettings::fromSettings( const QnEmailSettings &source ) {
    QnSimpleSmtpSettings result;
    result.email = source.email;
    result.password = source.password;
    result.signature = source.signature;
    result.supportEmail = source.supportEmail;
    return result;
}

QnSmtpSimpleSettingsWidget::QnSmtpSimpleSettingsWidget( QWidget* parent /*= nullptr*/ )
    : QWidget(parent)
    , ui(new Ui::SmtpSimpleSettingsWidget)
    , m_updating(false)
    , m_readOnly(false)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit,        &QLineEdit::textChanged,            this,   &QnSmtpSimpleSettingsWidget::validateEmail);

    setWarningStyle(ui->detectErrorLabel);

    /* Mark simple view mandatory fields. */
    declareMandatoryField(ui->emailLabel);
    declareMandatoryField(ui->passwordLabel);

    auto listenTo = [this](QLineEdit *lineEdit) {
        connect(lineEdit, &QLineEdit::textChanged, this, &QnSmtpSimpleSettingsWidget::settingsChanged);
    };

    listenTo(ui->emailLineEdit);
    listenTo(ui->passwordLineEdit);
    listenTo(ui->signatureLineEdit);
    listenTo(ui->supportLinkLineEdit);

    ui->supportLinkLineEdit->setPlaceholderText(QnAppInfo::supportLink());
}

QnSmtpSimpleSettingsWidget::~QnSmtpSimpleSettingsWidget()
{}

QnSimpleSmtpSettings QnSmtpSimpleSettingsWidget::settings() const {
    QnSimpleSmtpSettings result;
    result.email = ui->emailLineEdit->text(); 
    result.password = ui->passwordLineEdit->text();
    result.signature = ui->signatureLineEdit->text();
    result.supportEmail = ui->supportLinkLineEdit->text();
    return result;
}

void QnSmtpSimpleSettingsWidget::setSettings( const QnSimpleSmtpSettings &value ) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    ui->emailLineEdit->setText(value.email);
    ui->passwordLineEdit->setText(value.password);
    ui->signatureLineEdit->setText(value.signature);
    ui->supportLinkLineEdit->setText(value.supportEmail);
}

bool QnSmtpSimpleSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSmtpSimpleSettingsWidget::setReadOnly( bool readOnly ) {
    using ::setReadOnly;

    setReadOnly(ui->emailLineEdit, readOnly);
    setReadOnly(ui->passwordLineEdit, readOnly);
    setReadOnly(ui->signatureLineEdit, readOnly);
    setReadOnly(ui->supportLinkLineEdit, readOnly);
}

void QnSmtpSimpleSettingsWidget::validateEmail() {
    QString errorText;

    const QString targetEmail = ui->emailLineEdit->text();

    if (!targetEmail.isEmpty()) {
        QnEmailAddress email(targetEmail); 
        if (!email.isValid())
            errorText = tr("E-Mail is not valid");
        else if (email.smtpServer().isNull())
            errorText = tr("No preset found. Use 'Advanced' option.");
    }

    ui->detectErrorLabel->setText(errorText);
}

