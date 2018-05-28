#include "webpage_dialog.h"
#include "ui_webpage_dialog.h"

#include <QtCore/QUrl>

#include <core/resource/webpage_resource.h>

#include <nx/client/desktop/common/utils/aligner.h>
#include <ui/style/helper.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <nx/utils/log/assert.h>

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

using namespace nx::vms::api;

QnWebpageDialog::QnWebpageDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::WebpageDialog)
{
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
        ui->subTypeLabel
    });

    ui->subtypeComboBox->addItem(tr("None"), QVariant::fromValue(WebPageSubtype::none));
    ui->subtypeComboBox->addItem(lit("C2P"), QVariant::fromValue(WebPageSubtype::c2p));

    auto advancedCheckBox = new QCheckBox(ui->buttonBox);
    advancedCheckBox->setText(tr("Advanced"));
    ui->buttonBox->addButton(advancedCheckBox, QDialogButtonBox::ButtonRole::HelpRole);

    auto setAdvancedMode =
        [this](bool value)
        {
            ui->subtypeComboBox->setVisible(value);
            ui->subTypeLabel->setVisible(value);
        };

    connect(advancedCheckBox, &QCheckBox::stateChanged, this,
        [setAdvancedMode](int state) { setAdvancedMode(state == Qt::Checked); });
    setAdvancedMode(false);

    connect(ui->subtypeComboBox, QnComboboxCurrentIndexChanged, this,
        [advancedCheckBox](int index) { if (index != 0) advancedCheckBox->setChecked(true); });

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
    return ui->subtypeComboBox->currentData().value<WebPageSubtype>();
}

void QnWebpageDialog::setSubtype(WebPageSubtype value)
{
    const int index = ui->subtypeComboBox->findData(QVariant::fromValue(value));
    NX_EXPECT(index == static_cast<int>(value));
    ui->subtypeComboBox->setCurrentIndex(index);
}

void QnWebpageDialog::accept()
{
    if (ui->urlInputField->validate())
        base_type::accept();
}
