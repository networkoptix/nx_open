#include "exec_http_request_action_widget.h"
#include "ui_exec_http_request_action_widget.h"

#include <nx/vms/event/action_parameters.h>
#include <utils/common/scoped_value_rollback.h>

#include <map>

namespace
{

static const int kAutoContentTypeItemIndex = 0;
static const int kAutoAuthTypeItemIndex = 0;
static const int kAutoRequestTypeItemIndex = 0;
static const std::map <nx_http::Method::ValueType, int> kAutoRequestTypeItemIndexMap
    { {"", 0}, {"GET", 1}, {"POST", 2} };

} // namespace

QnExecHttpRequestActionWidget::QnExecHttpRequestActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ExecHttpRequestActionWidget)
{
    ui->setupUi(this);

    connect(ui->httpUrlLineEdit,      &QLineEdit::textChanged,         this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->loginLineEdit,        &QLineEdit::textChanged,         this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->passwordLineEdit,     &QLineEdit::textChanged,         this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->contentTextEdit,      &QPlainTextEdit::textChanged,    this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->contentTypeComboBox,  &QComboBox::currentTextChanged,  this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->authTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &QnExecHttpRequestActionWidget::paramsChanged);
    connect(ui->requestTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &QnExecHttpRequestActionWidget::paramsChanged);

    ui->contentTypeComboBox->addItem(tr("Auto")); //< should have kAutoContentTypeItemIndex position.
    ui->contentTypeComboBox->addItem(lit("text/plain"));
    ui->contentTypeComboBox->addItem(lit("text/html"));
    ui->contentTypeComboBox->addItem(lit("application/html"));
    ui->contentTypeComboBox->addItem(lit("application/json"));
    ui->contentTypeComboBox->addItem(lit("application/xml"));
    NX_ASSERT(ui->contentTypeComboBox->itemText(kAutoContentTypeItemIndex) == tr("Auto"));

    ui->authTypeComboBox->addItem(tr("Auto")); //< should have kAutoAuthTypeItemIndex position.
    ui->authTypeComboBox->addItem(lit("Digest"));
    ui->authTypeComboBox->addItem(lit("Basic"));
    NX_ASSERT(ui->authTypeComboBox->itemText(kAutoAuthTypeItemIndex) == tr("Auto"));

    ui->requestTypeComboBox->addItem(tr("Auto")); //< should have kAutoRequestTypeItemIndex position.
    ui->requestTypeComboBox->addItem(lit("GET"));
    ui->requestTypeComboBox->addItem(lit("POST"));
    NX_ASSERT(ui->requestTypeComboBox->itemText(kAutoRequestTypeItemIndex) == tr("Auto"));
}

QnExecHttpRequestActionWidget::~QnExecHttpRequestActionWidget()
{}

void QnExecHttpRequestActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->httpUrlLineEdit);
    setTabOrder(ui->httpUrlLineEdit, ui->contentTextEdit);
    setTabOrder(ui->contentTextEdit, ui->contentTypeComboBox);
    setTabOrder(ui->contentTypeComboBox, ui->loginLineEdit);
    setTabOrder(ui->loginLineEdit, ui->passwordLineEdit);
    setTabOrder(ui->passwordLineEdit, ui->authTypeComboBox);
    setTabOrder(ui->authTypeComboBox, ui->requestTypeComboBox);
    setTabOrder(ui->requestTypeComboBox, after);
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
        ui->contentTypeComboBox->setCurrentIndex(kAutoContentTypeItemIndex);
    else
        ui->contentTypeComboBox->setCurrentText(params.contentType);
    ui->httpUrlLineEdit->setText(url.toString(QUrl::RemoveUserInfo));
    ui->loginLineEdit->setText(url.userName());
    ui->passwordLineEdit->setText(url.password());

    ui->authTypeComboBox->setCurrentIndex(static_cast<int>(params.authType));

    auto it=kAutoRequestTypeItemIndexMap.find(params.requestType);
    int requestTypeComboBoxIndex = (it != kAutoRequestTypeItemIndexMap.end()) ? it->second : 0;
    ui->requestTypeComboBox->setCurrentIndex(requestTypeComboBoxIndex);
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
    params.contentType = ui->contentTypeComboBox->currentText().trimmed();
    if (params.contentType == ui->contentTypeComboBox->itemText(kAutoContentTypeItemIndex))
        params.contentType.clear(); //< Auto value

    params.authType = static_cast<nx_http::AsyncHttpClient::AuthType>(ui->authTypeComboBox->currentIndex());

    params.requestType = ui->requestTypeComboBox->currentText().toLatin1();
    if (params.requestType == ui->requestTypeComboBox->itemText(kAutoRequestTypeItemIndex).toLatin1())
        params.requestType.clear(); //< Auto value

    model()->setActionParams(params);
}
