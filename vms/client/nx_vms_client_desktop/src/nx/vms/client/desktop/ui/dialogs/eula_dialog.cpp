#include "eula_dialog.h"
#include "ui_eula_dialog.h"

#include <QtCore/QFile>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>

#include <client/client_app_info.h>
#include <client/client_settings.h>
#include <ui/style/custom_style.h>
#include <ui/style/webview_style.h>
#include <ui/style/skin.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kDesiredSize{800, 600};

} // namespace

EulaDialog::EulaDialog(QWidget* parent):
    base_type(parent, Qt::Dialog), //< Don't set Qt::MSWindowsFixedSizeDialogHint, it blocks resize.
    ui(new Ui::EulaDialog())
{
    ui->setupUi(this);
    ui->iconLabel->setPixmap(
        QnSkin::maximumSizePixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning)));

    auto font = ui->titleLabel->font();
    font.setPixelSize(font.pixelSize() + 2);
    ui->titleLabel->setFont(font);
    ui->titleLabel->setForegroundRole(QPalette::Light);

    //ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    // We have replaced standard buttons to keep our own button order and style.

    connect(ui->accept, &QPushButton::clicked, this,
        [this]()
        {
            done(QDialog::DialogCode::Accepted);
        });

    connect(ui->reject, &QPushButton::clicked, this,
        [this]()
        {
            done(QDialog::DialogCode::Rejected);
        });

    setAccentStyle(ui->accept);

    NxUi::setupWebViewStyle(ui->eulaView, NxUi::WebViewStyle::eula);
}

EulaDialog::~EulaDialog()
{
    // Here for forward-declared scoped pointer destruction.
}

QString EulaDialog::title() const
{
    return ui->titleLabel->text();
}

void EulaDialog::setTitle(const QString& value)
{
    ui->titleLabel->setText(value);
}

void EulaDialog::setEulaHtml(const QString& html)
{
    const auto eulaHtmlStyle = NxUi::generateCssStyle();

    auto eulaText = html;
    eulaText.replace(
        lit("<head>"),
        lit("<head><style>%1</style>").arg(eulaHtmlStyle));

    ui->eulaView->setHtml(eulaText);
    // We do not want to copy embedded style to clipboard.
    ui->copyToClipboard->setClipboardText(html);
}

bool EulaDialog::hasHeightForWidth() const
{
    return false;
}

void EulaDialog::updateSize()
{
    const auto window = windowHandle();
    if (!window)
        return;

    const auto maximumSize = window->screen()->availableSize()
        - client::core::Geometry::sizeDelta(window->frameMargins());

    setFixedSize(kDesiredSize.boundedTo(maximumSize));
    window->setMaximumSize(kDesiredSize.boundedTo(maximumSize));
}

bool EulaDialog::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Show:
        {
            const auto window = windowHandle();
            if (!window)
                break;

            connect(window, &QWindow::screenChanged, this, &EulaDialog::updateSize,
                Qt::UniqueConnection);

            updateSize();
            window->setFramePosition(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                window->frameGeometry().size(), window->screen()->availableGeometry()).topLeft());

            break;
        }

        default:
            break;
    }

    return base_type::event(event);
}

bool EulaDialog::acceptEulaHtml(const QString& html, int version, QWidget* parent)
{
    // Regexp to dig out a title from html with EULA.
    QRegExp headerRegExp("<title>(.+)</title>", Qt::CaseInsensitive);
    headerRegExp.setMinimal(true);

    QString eulaHeader;
    if (headerRegExp.indexIn(html) != -1)
    {
        QString title = headerRegExp.cap(1);
        eulaHeader = tr("Please review and agree to the %1 in order to proceed").arg(title);
    }
    else
    {
        // We are here only if some vile monster has chewed our eula file.
        NX_ASSERT(false);
        eulaHeader = tr("To use the software you must agree with the end user license agreement");
    }

    EulaDialog eulaDialog(parent);
    eulaDialog.setTitle(eulaHeader);
    eulaDialog.setEulaHtml(html);

    if (eulaDialog.exec() == QDialog::DialogCode::Accepted)
    {
        qnSettings->setAcceptedEulaVersion(version);
        return true;
    }
    return false;
}

bool EulaDialog::acceptEulaFromFile(const QString& path, int version, QWidget* parent)
{
    if (!NX_ASSERT(!path.isEmpty()))
        return false;

    QFile eula(path);
    if (!eula.open(QIODevice::ReadOnly))
    {
        NX_ERROR(typeid(EulaDialog), "Failed to open eula file %1", path);
        return false;
    }

    const auto eulaText = QString::fromUtf8(eula.readAll());
    return acceptEulaHtml(eulaText, version, parent);
}

} // namespace nx::vms::client::desktop
