#include "exec_http_request_action_widget.h"
#include "ui_exec_http_request_action_widget.h"

#include <business/business_action_parameters.h>
#include <utils/common/scoped_value_rollback.h>

QnExecHttpRequestActionWidget::QnExecHttpRequestActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ExecHttpRequestActionWidget)
{
    ui->setupUi(this);

    connect(ui->httpUrlLineEdit,      &QLineEdit::textChanged,       this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->loginLineEdit,        &QLineEdit::textChanged,       this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->passwordLineEdit,     &QLineEdit::textChanged,       this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->contentTextEdit,      &QPlainTextEdit::textChanged,  this, &QnExecHttpRequestActionWidget::paramsChanged);
}

QnExecHttpRequestActionWidget::~QnExecHttpRequestActionWidget()
{}

void QnExecHttpRequestActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->httpUrlLineEdit);
    setTabOrder(ui->httpUrlLineEdit, ui->loginLineEdit);
    setTabOrder(ui->loginLineEdit, ui->passwordLineEdit);
    setTabOrder(ui->passwordLineEdit, ui->contentTextEdit);
    setTabOrder(ui->contentTextEdit, after);
}

void QnExecHttpRequestActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    const auto params = model()->actionParams();
    QUrl url(params.url);
    ui->contentTextEdit->setPlainText(params.text);
    ui->httpUrlLineEdit->setText(url.toString(QUrl::RemoveUserInfo));
    ui->loginLineEdit->setText(url.userName());
    ui->passwordLineEdit->setText(url.password());
}

void QnExecHttpRequestActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();

    QUrl url(ui->httpUrlLineEdit->text());
    url.setUserName(ui->loginLineEdit->text());
    url.setPassword(ui->passwordLineEdit->text());
    if (url.scheme().isEmpty())
        url.setScheme(lit("http"));

    params.url = url.toString();
    params.text = ui->contentTextEdit->toPlainText();
    model()->setActionParams(params);
}
