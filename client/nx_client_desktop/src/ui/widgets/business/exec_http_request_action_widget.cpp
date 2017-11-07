#include "exec_http_request_action_widget.h"
#include "ui_exec_http_request_action_widget.h"

#include <nx/vms/event/action_parameters.h>
#include <utils/common/scoped_value_rollback.h>

namespace
{

static const int kAutoContentItemIndex = 0;

} // namespace

QnExecHttpRequestActionWidget::QnExecHttpRequestActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ExecHttpRequestActionWidget)
{
    ui->setupUi(this);

    connect(ui->httpUrlLineEdit,      &QLineEdit::textChanged,        this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->loginLineEdit,        &QLineEdit::textChanged,        this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->passwordLineEdit,     &QLineEdit::textChanged,        this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->contentTextEdit,      &QPlainTextEdit::textChanged,   this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->comboBoxContentType,  &QComboBox::currentTextChanged, this, &QnExecHttpRequestActionWidget::paramsChanged);

    ui->comboBoxContentType->addItem(tr("Auto")); //< should have kAutoContentItemIndex position.

    ui->comboBoxContentType->addItem(lit("text/plain"));
    ui->comboBoxContentType->addItem(lit("text/html"));
    ui->comboBoxContentType->addItem(lit("application/html"));
    ui->comboBoxContentType->addItem(lit("application/json"));
    ui->comboBoxContentType->addItem(lit("application/xml"));

    NX_ASSERT(ui->comboBoxContentType->itemText(kAutoContentItemIndex) == tr("Auto"));
}

QnExecHttpRequestActionWidget::~QnExecHttpRequestActionWidget()
{}

void QnExecHttpRequestActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->httpUrlLineEdit);
    setTabOrder(ui->httpUrlLineEdit, ui->loginLineEdit);
    setTabOrder(ui->loginLineEdit, ui->passwordLineEdit);
    setTabOrder(ui->passwordLineEdit, ui->contentTextEdit);
    setTabOrder(ui->contentTextEdit, ui->comboBoxContentType);
    setTabOrder(ui->comboBoxContentType, after);
}

void QnExecHttpRequestActionWidget::at_model_dataChanged(Fields fields)
{
    Q_UNUSED(fields);
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    const auto params = model()->actionParams();
    QUrl url(params.url);
    ui->contentTextEdit->setPlainText(params.text);
    if (params.contentType.isEmpty())
        ui->comboBoxContentType->setCurrentIndex(kAutoContentItemIndex);
    else
        ui->comboBoxContentType->setCurrentText(params.contentType);
    ui->httpUrlLineEdit->setText(url.toString(QUrl::RemoveUserInfo));
    ui->loginLineEdit->setText(url.userName());
    ui->passwordLineEdit->setText(url.password());
}

void QnExecHttpRequestActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    auto params = model()->actionParams();

    QUrl url(ui->httpUrlLineEdit->text());
    url.setUserName(ui->loginLineEdit->text());
    url.setPassword(ui->passwordLineEdit->text());
    if (url.scheme().isEmpty())
        url.setScheme(lit("http"));

    params.url = url.toString();
    params.text = ui->contentTextEdit->toPlainText();
    params.contentType = ui->comboBoxContentType->currentText().trimmed();
    if (params.contentType == ui->comboBoxContentType->itemText(kAutoContentItemIndex))
        params.contentType.clear(); //< Auto value
    model()->setActionParams(params);
}
