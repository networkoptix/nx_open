#include "webpage_dialog.h"
#include "ui_webpage_dialog.h"

#include <QtCore/QUrl>

#include <nx/client/desktop/common/utils/aligner.h>

using namespace nx::client::desktop;

namespace {

bool isValidUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    const static QString kHttp(lit("http"));
    const static QString kHttps(lit("https"));

    return url.scheme() == kHttp || url.scheme() == kHttps;
}

} // namespace

QnWebpageDialog::QnWebpageDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::WebpageDialog)
{
    ui->setupUi(this);

    ui->nameInputField->setTitle(tr("Name"));

    ui->urlInputField->setTitle(tr("URL"));
    ui->urlInputField->setValidator(
        [this](const QString& url)
        {
            if (url.trimmed().isEmpty())
                return ValidationResult(tr("URL cannot be empty."));

            return isValidUrl(QUrl::fromUserInput(url))
                ? ValidationResult::kValid
                : ValidationResult(tr("Wrong URL format."));
        });

    connect(ui->urlInputField, &InputField::textChanged, this,
        [this](const QString& text)
        {
            const auto url = this->url();
            ui->nameInputField->setPlaceholderText(url.isEmpty() ? tr("Web Page") : url);
        });

    ui->urlInputField->setPlaceholderText(lit("example.org"));

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->nameInputField,
        ui->urlInputField });

    setResizeToContentsMode(Qt::Vertical);
}

QnWebpageDialog::~QnWebpageDialog()
{
}

QString QnWebpageDialog::name() const
{
    return ui->nameInputField->text();
}

QString QnWebpageDialog::url() const
{
    return ui->urlInputField->text().trimmed();
}

void QnWebpageDialog::setName(const QString& name)
{
    ui->nameInputField->setText(name);
}

void QnWebpageDialog::setUrl(const QString& url)
{
    ui->urlInputField->setText(url.trimmed());
}

void QnWebpageDialog::accept()
{
    if (ui->urlInputField->validate())
        base_type::accept();
}
