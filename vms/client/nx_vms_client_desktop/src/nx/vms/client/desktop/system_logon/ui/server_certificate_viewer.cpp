// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_viewer.h"
#include "ui_server_certificate_viewer.h"

#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTreeWidgetItem>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/network/socket_common.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>

namespace {

QString name(const nx::network::ssl::X509Name& subject, const QString& defaultValue = {})
{
    if (const auto optValue = nx::vms::client::core::certificateName(subject))
        return QString::fromStdString(*optValue);

    return defaultValue;
}

QString highlightedText(const QString& text)
{
    const auto color = nx::vms::client::desktop::colorTheme()->color("light10");
    return QString("<span style=\"color: %1;\">%2</span>").arg(color.name()).arg(text);
}

} // namespace

namespace nx::vms::client::desktop {

ServerCertificateViewer::ServerCertificateViewer(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const std::vector<nx::network::ssl::Certificate>& certificates,
    Mode mode,
    QWidget *parent)
    :
    ServerCertificateViewer(parent)
{
    NX_ASSERT(mode != Mode::mismatch); //< Resource pointer is required for mismatch mode.
    setCertificateData(target, primaryAddress, certificates, mode);
}

ServerCertificateViewer::ServerCertificateViewer(
    const QnMediaServerResourcePtr& server,
    const std::vector<nx::network::ssl::Certificate>& certificates,
    Mode mode,
    QWidget *parent)
    :
    ServerCertificateViewer(parent)
{
    m_server = server;
    setCertificateData(
        server->getModuleInformation(),
        server->getPrimaryAddress(),
        certificates,
        mode);
}

ServerCertificateViewer::ServerCertificateViewer(
    QWidget *parent)
    :
    base_type(parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::ServerCertificateViewer())
{
    ui->setupUi(this);

    const int kScrollBarWidth = 6;
    const int kCertificateChainLineHeight = 20;
    const int kSubjectFontSize = 22;

    // Set up certificate chain part of viewer.
    const auto color = nx::vms::client::desktop::colorTheme()->color("dark10");
    const auto kLineStyle = QString("border: none; background-color: %1;").arg(color.name());
    ui->treeTopLine->setStyleSheet(kLineStyle);
    ui->treeBottomLine->setStyleSheet(kLineStyle);
    ui->treeWidget->setItemsExpandable(false);
    ui->treeWidget->setFocusPolicy(Qt::NoFocus);
    ui->treeWidget->verticalScrollBar()->setMaximumWidth(kScrollBarWidth);

    // Decrease line height used by tree widget.
    auto delegate = new CustomizableItemDelegate(this);
    delegate->setCustomSizeHint(
        [delegate](const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            auto sizeHint = delegate->baseSizeHint(option, index);
            sizeHint.setHeight(kCertificateChainLineHeight);
            return sizeHint;
        });
    ui->treeWidget->setItemDelegate(delegate);

    // Set up tab widget.
    ui->tabWidget->tabBar()->setProperty(nx::style::Properties::kTabShape,
        static_cast<int>(nx::style::TabShape::Compact));

    // Set up General tab.
    auto aligner = new nx::vms::client::desktop::Aligner(this);
    aligner->addWidgets({ui->issuedByTextLabel, ui->expiresTextLabel});

    ui->certificateIcon->setPixmap(
        qnSkin->pixmap("misc/certificate.svg",
        true,
        ui->certificateIcon->size()));

    auto font = ui->subject->font();
    font.setPixelSize(kSubjectFontSize);
    font.setWeight(QFont::Medium);
    ui->subject->setFont(font);
    ui->subject->setForegroundRole(QPalette::Text);

    ui->expiresValueLabel->setForegroundRole(QPalette::Light);
    ui->issuedByValueLabel->setForegroundRole(QPalette::Light);
    ui->sha1->setForegroundRole(QPalette::Light);
    ui->sha256->label()->setForegroundRole(QPalette::Light);

    setWarningStyle(ui->expiredLabel);
    setWarningStyle(ui->selfSignedLabel);

    // Set up the warning banner.
    ui->warningLabel->setForegroundRole(QPalette::Text);
    ui->pinCertificateButton->setIcon(qnSkin->icon("misc/pin_text_icon.svg"));
    ui->viewPinnedButton->setIcon(qnSkin->icon("misc/certificate_text_icon.svg"));
    setPaletteColor(ui->warningBanner, QPalette::Window, colorTheme()->color("dialog.alertBar"));
    ui->warningBanner->setAutoFillBackground(true);

    // Due to layout flaws caused by QTreeWidgets, dialog size constraints should be set through
    // its child element.
    ui->line->setMinimumSize(500, 0);

    // Connect signals.
    connect(
        ui->treeWidget, &QTreeWidget::currentItemChanged,
        this, &ServerCertificateViewer::showSelectedCertificate);
    connect(
        ui->pinCertificateButton, &QPushButton::clicked,
        this, &ServerCertificateViewer::pinCertificate);
    connect(
        ui->viewPinnedButton, &QPushButton::clicked,
        this, &ServerCertificateViewer::showPinnedCertificate);
}

ServerCertificateViewer::~ServerCertificateViewer()
{
}

void ServerCertificateViewer::showEvent(QShowEvent *event)
{
    buttonBox()->setFocus();
    base_type::showEvent(event);
}

void ServerCertificateViewer::setCertificateData(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const std::vector<nx::network::ssl::Certificate>& certificates,
    Mode mode)
{
    NX_ASSERT(!certificates.empty());
    m_certificates = nx::network::ssl::completeCertificateChain(certificates);

    setWindowTitle(calculateDialogTitle(mode));

    // Update text labels with server description.
    ui->serverInfoLabel->setText(calculateServerInfo(mode, target, primaryAddress));
    ui->serverIdLabel->setText(
        tr("Server Id: %1").arg(highlightedText(target.id.toSimpleString())));

    // Update warning message (even if it would not be shown).
    ui->warningLabel->setText(
        tr("This certificate does not match the certificate %1 is pinned to.").arg(target.name));

    // Update tree widget.
    ui->treeWidget->clear();
    QTreeWidgetItem* lastItem = ui->treeWidget->invisibleRootItem();
    for (int i = m_certificates.size(); i --> 0;)
    {
        const auto& cert = m_certificates[i];
        auto newItem = new QTreeWidgetItem(lastItem);
        newItem->setIcon(0, qnSkin->icon("misc/certificate_icon.svg"));
        newItem->setText(0, name(cert.subject()));
        newItem->setData(0, Qt::UserRole, i); //< Store certificate index as UserRole data.
        lastItem = newItem;
    }
    ui->treeWidget->expandAll();
    ui->treeWidget->setRootIsDecorated(m_certificates.size() > 1);
    ui->treeWidget->setFixedHeight(
        ui->treeWidget->sizeHintForRow(0) * qMin<int>(3, m_certificates.size()));

    // The warning banner affects layout of certificate view tabs: the General tab should have
    // some free space at the bottom when the banner is hidden, while tab Details should use
    // the whole avaliable space. One of the possible solutions here is to set the minimum size
    // of some tab depending on the banner state.
    auto minHeight = ui->detailsTab->sizeHint().height();
    if (mode != Mode::mismatch)
        minHeight += ui->warningBanner->height();
    ui->detailsTab->setMinimumHeight(minHeight);
    ui->warningBanner->setVisible(mode == Mode::mismatch);

    // Select and show the user certificate (last one in our chain).
    ui->treeWidget->setCurrentItem(lastItem);
}

void ServerCertificateViewer::showSelectedCertificate()
{
    // Retrieve certificate index from the selected item.
    const auto currentItem = ui->treeWidget->currentItem();
    if (!NX_ASSERT(currentItem))
        return;
    bool ok = false;
    int index = currentItem->data(0, Qt::UserRole).toInt(&ok);
    if (!NX_ASSERT(ok && index >= 0 && index < m_certificates.size()))
        return;

    // Update data.
    const auto& cert = m_certificates[index];
    const auto expiresAt = duration_cast<std::chrono::seconds>(cert.notAfter().time_since_epoch());

    ui->subject->setText(name(cert.subject()));

    ui->issuedByValueLabel->setText(name(cert.issuer()));
    ui->expiresValueLabel->setText(
        QDateTime::fromSecsSinceEpoch(expiresAt.count()).toString());

    ui->sha1->setText(nx::vms::client::core::formattedFingerprint(cert.sha1()));
    ui->sha256->setText(nx::vms::client::core::formattedFingerprint(cert.sha256()));

    ui->certificateText->setText(QString::fromStdString(cert.printedText()));

    // Update certificate warnings state.
    ui->issuedWidget->setCurrentWidget(
        cert.issuer() == cert.subject() ? ui->selfSignedPage : ui->issuerPage);
    ui->expiresWidget->setCurrentWidget(
        expiresAt.count() > QDateTime::currentSecsSinceEpoch() ? ui->expiresPage : ui->expiredPage);
}

void ServerCertificateViewer::pinCertificate()
{
    if (!NX_ASSERT(m_server))
        return;

    const auto name = m_server->getName();

    QnMessageBox message(this);
    message.setIcon(QnMessageBox::Icon::Question);
    message.setText(tr("Pin this certificate to %1?").arg(name));
    message.setInformativeText(
        tr("Someone may be impersonating %1 to steal your personal information.\n"
            "Do not pin this certificate if you didn't modify %2 server SSL certificate.")
            .arg(name, name));
    message.addButton(
        tr("Pin"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);
    message.setStandardButtons({QDialogButtonBox::Cancel});

    message.button(QDialogButtonBox::Cancel)->setFocus();
    message.exec();

    // Message box uses both standard and custom buttons. Check button role instead of result code.
    if (message.buttonRole(message.clickedButton()) != QDialogButtonBox::AcceptRole)
        return;

    // Update server's certificate property.
    std::string pem;
    for (const auto& cert: m_certificates)
        pem += nx::network::ssl::X509Certificate(cert.x509()).pemString();
    qnResourcesChangesManager->saveServer(
        m_server,
        [pem=QString::fromStdString(pem)](const auto& server)
        {
            server->setProperty(ResourcePropertyKey::Server::kCertificate, pem);
        });

    // Close the dialog.
    accept();
}

void ServerCertificateViewer::showPinnedCertificate()
{
    if (!NX_ASSERT(m_server))
        return;

    auto pinnedViewer = new ServerCertificateViewer(
        m_server,
        nx::network::ssl::Certificate::parse(m_server->certificate()),
        Mode::pinned,
        this);

    // We want to make both dialogs simultaneously available. To achieve this we show the child
    // certificate viewer as non-modal window and disable 'Show' button until the child is closed.
    pinnedViewer->setModal(false);
    ui->viewPinnedButton->setEnabled(false);
    connect(
        pinnedViewer, &QDialog::finished,
        this, [this]{ ui->viewPinnedButton->setEnabled(true); });
    connect(
        this, &QDialog::finished,
        pinnedViewer, &QDialog::reject);

    // Show child window with an offset.
    pinnedViewer->move(pos().x() + 40, pos().y() + 40);
    pinnedViewer->show();
}

QString ServerCertificateViewer::calculateDialogTitle(Mode mode)
{
    switch (mode)
    {
        case Mode::custom:
            return tr("Custom certificate");
        case Mode::pinned:
            return tr("Auto-generated certificate");
        default:
            return tr("Unknown certificate");
    }
}

QString ServerCertificateViewer::calculateServerInfo(
    Mode mode,
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress)
{
    const QString serverInfo = highlightedText(nx::format(
        "%1 (%2)", target.name, primaryAddress.address.toString()));

    switch (mode)
    {
        case Mode::custom:
            return tr("This is a custom certificate installed on %1").arg(serverInfo);
        case Mode::pinned:
            return tr("The certificate is auto-generated and pinned to %1").arg(serverInfo);
        default:
            return tr("The certificate was presented by %1").arg(serverInfo);
    }
}

} // namespace nx::vms::client::desktop
