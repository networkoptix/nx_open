#include "webpage_dialog.h"
#include "ui_webpage_dialog.h"

#include <QtCore/QUrl>

#include <core/resource/webpage_resource.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/network/http/http_types.h>

namespace {

bool isValidUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    return nx::network::http::isUrlSheme(url.scheme());
}

} // namespace

using namespace nx::vms::api;

QnWebpageDialog::QnWebpageDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::WebpageDialog)
{
    using namespace nx::vms::client::desktop;

    ui->setupUi(this);

    ui->nameInputField->setTitle(tr("Name"));

    ui->urlInputField->setTitle(tr("URL"));
    ui->urlInputField->setValidator(
        [](const QString& url)
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
            const auto url = this->url().toString();
            ui->nameInputField->setPlaceholderText(url.isEmpty() ? tr("Web Page") : url);
        });

    ui->urlInputField->setPlaceholderText(lit("example.org"));

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->nameInputField,
        ui->urlInputField,
        ui->c2pCheckBoxSpacerWidget
    });

    setResizeToContentsMode(Qt::Vertical);
}

QnWebpageDialog::~QnWebpageDialog()
{
}

QString QnWebpageDialog::name() const
{
    const auto name = ui->nameInputField->text().trimmed();
    return name.isEmpty()
        ? QnWebPageResource::nameForUrl(url())
        : name;
}

QUrl QnWebpageDialog::url() const
{
    return QUrl::fromUserInput(ui->urlInputField->text().trimmed());
}

void QnWebpageDialog::setName(const QString& name)
{
    ui->nameInputField->setText(name);
}

void QnWebpageDialog::setUrl(const QUrl& url)
{
    ui->urlInputField->setText(url.toString());
}

WebPageSubtype QnWebpageDialog::subtype() const
{
    return ui->c2pCheckBox->isChecked()
        ? WebPageSubtype::c2p
        : WebPageSubtype::none;
}

void QnWebpageDialog::setSubtype(WebPageSubtype value)
{
    ui->c2pCheckBox->setChecked(value == WebPageSubtype::c2p);
}

void QnWebpageDialog::accept()
{
    if (ui->urlInputField->validate())
        base_type::accept();
}
