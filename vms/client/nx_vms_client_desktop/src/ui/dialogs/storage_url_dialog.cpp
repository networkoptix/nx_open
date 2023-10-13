// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_url_dialog.h"
#include "ui_storage_url_dialog.h"

#include <algorithm>

#include <QtCore/QCryptographicHash>

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <core/resource/abstract_storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/storage_init_result.h>
#include <nx/vms/client/desktop/common/utils/scoped_cursor_rollback.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <server/server_storage_manager.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::vms::client::desktop;

QnStorageUrlDialog::QnStorageUrlDialog(
    const QnMediaServerResourcePtr& server,
    QnServerStorageManager* storageManager,
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    ui(new Ui::StorageUrlDialog()),
    m_storageManager(storageManager),
    m_server(server),
    m_okButton(new BusyIndicatorButton(this))
{
    NX_ASSERT(m_storageManager);
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::ServerSettings_Storages);
    ui->urlEdit->setFocus();
    connect(ui->protocolComboBox, QnComboboxCurrentIndexChanged, this,
        &QnStorageUrlDialog::at_protocolComboBox_currentIndexChanged);

    /* Replace OK button with a busy indicator button: */
    QScopedPointer<QAbstractButton> baseOkButton(ui->buttonBox->button(QDialogButtonBox::Ok));
    m_okButton->setText(baseOkButton->text()); // Title from OS theme
    m_okButton->setIcon(baseOkButton->icon()); // Icon from OS theme
    setAccentStyle(m_okButton);
    ui->buttonBox->removeButton(baseOkButton.data());
    ui->buttonBox->addButton(m_okButton, QDialogButtonBox::AcceptRole);

    /* Override cursor to stay arrow when entire dialog has wait cursor: */
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);

    setResizeToContentsMode(Qt::Vertical | Qt::Horizontal);
}

QnStorageUrlDialog::~QnStorageUrlDialog()
{
}

QSet<QString> QnStorageUrlDialog::protocols() const
{
    return m_protocols;
}

void QnStorageUrlDialog::setProtocols(const QSet<QString>& protocols)
{
    if (m_protocols == protocols)
        return;

    m_protocols = protocols;

    m_descriptions.clear();

    QStringList sortedProtocols(m_protocols.values());
    sortedProtocols.sort(Qt::CaseInsensitive);

    for (const QString& protocol: sortedProtocols)
    {
        ProtocolDescription description = protocolDescription(protocol);
        if (!description.name.isNull())
            m_descriptions.push_back(description);
    }

    updateComboBox();
}

QnStorageModelInfo QnStorageUrlDialog::storage() const
{
    return m_storage;
}

nx::vms::api::Credentials QnStorageUrlDialog::credentials() const
{
    return nx::vms::api::Credentials(ui->loginLineEdit->text(), ui->passwordLineEdit->text());
}

QString QnStorageUrlDialog::normalizePath(QString path)
{
    QString separator = lit("/");
    //const auto data = runtimeInfoManager()->item(m_server->getId()).data;
    //if (data.platform.toLower() == lit("windows"))
    //    separator = lit("\\");
    QString result = path.replace('/', separator);
    result = path.replace('\\', separator);
    if (result.endsWith(separator))
        result.chop(1);
    if (separator == lit("/"))
    {
        int i = 0;
        while (result[i] == separator[0])
            ++i;
        return result.mid(i);
    }
    return result;
}

QnStorageUrlDialog::ProtocolDescription QnStorageUrlDialog::protocolDescription(const QString& protocol)
{
    ProtocolDescription result;

    QString validIpPattern = lit("(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])");
    QString validHostnamePattern = lit("(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])");

    if (protocol == lit("smb"))
    {
        result.protocol = protocol;
        result.name = tr("Network Shared Resource");
        result.urlTemplate = tr("\\\\<Computer Name>\\<Folder>");
        result.urlPattern = lit("\\\\\\\\%1\\\\.+").arg(validHostnamePattern);
    }
    //else if (protocol == lit("coldstore"))
    //{
    //    result.protocol = protocol;
    //    result.name = tr("Coldstore Network Storage");
    //    result.urlTemplate = tr("coldstore://<Address>");
    //    result.urlPattern = lit("coldstore://%1/?").arg(validHostnamePattern);
    //}
    else if (protocol != lit("file") && protocol != lit("local"))
    {
        result.protocol = protocol;
        result.name = protocol.toUpper();
        result.urlTemplate = lit("%1://<Address>/path").arg(protocol);
        result.urlPattern = lit("%1://%2").arg(protocol).arg(validHostnamePattern);
    }

    return result;
}

QString QnStorageUrlDialog::makeUrl(const QString& path, const QString& login, const QString& password)
{
    QString urlString = normalizePath(path);

    QUrl url{urlString};
    if (url.scheme().isEmpty())
        url.setScheme("smb");

    if (!login.isEmpty())
        url.setUserName(login);

    if (!password.isEmpty())
        url.setPassword(password);

    // Do not use the QUrl::toPercentEncoding method here, as if user info, e.g., 'user:passwd%',
    // contains a '%' character, it is encoded twice. The first time on url parsing, it becomes
    // equal to 'user:passwd%25'. The second on QUrl::toPercentEncoding, it becomes equal to
    // 'user%3Apasswd%2525'.
    return url.toEncoded();
}

void QnStorageUrlDialog::atStorageStatusReply(const StorageStatusReply& reply)
{
    m_reply = reply;
}

void QnStorageUrlDialog::accept()
{
    const auto protocol = ui->protocolComboBox->currentData().toString();
    auto urlText = ui->urlEdit->text().trimmed();

    if (protocol == "smb")
    {
        constexpr auto kSmbPathPrefix = R"(\\)";
        if (!urlText.startsWith(kSmbPathPrefix))
            urlText = kSmbPathPrefix + urlText;
    }
    else if (!urlText.toUpper().startsWith(protocol.toUpper() + "://"))
    {
        urlText = protocol.toLower() + "://" + urlText;
    }

    enum {
        kFinished,
        kCancelled
    };

    QSharedPointer<QEventLoop> loop(new QEventLoop());
    QString url = makeUrl(urlText, ui->loginLineEdit->text(), ui->passwordLineEdit->text());
    m_storageManager->requestStorageStatus(m_server, url,
        [loop, tool = QPointer<QnStorageUrlDialog>(this)](
            const StorageStatusReply& reply)
        {
            if (tool)
                tool->atStorageStatusReply(reply);

            loop->exit(kFinished);
        });

    connect(this, &QDialog::rejected, loop.get(),
        [loop]() { loop->exit(kCancelled); });

    // Scoped event loop:
    {
        ScopedCursorRollback cursorRollback(this, Qt::WaitCursor);
        QnScopedTypedPropertyRollback<bool, QWidget> inputsEnabledRollback(ui->inputsWidget,
            &QWidget::setEnabled, &QWidget::isEnabled, false);
        QnScopedTypedPropertyRollback<bool, QWidget> buttonEnabledRollback(m_okButton,
            &QWidget::setEnabled, &QWidget::isEnabled, false);
        QnScopedTypedPropertyRollback<bool, BusyIndicatorButton> buttonIndicatorRollback(m_okButton,
            &BusyIndicatorButton::showIndicator, &BusyIndicatorButton::isIndicatorVisible, true);

        if (loop->exec() == kCancelled)
            return;
    }

    switch (m_reply.status)
    {
        case Qn::StorageInit_CreateFailed:
        case Qn::StorageInit_WrongPath:
            QnMessageBox::warning(this, tr("Invalid storage path"));
            return;
        case Qn::StorageInit_WrongAuth:
            QnMessageBox::warning(this, tr("Invalid credentials for external storage"));
            return;
        case Qn::StorageInit_Ok:
            if (!NX_ASSERT(m_reply.storage.isOnline))
            {
                QnMessageBox::warning(this, tr("Invalid storage path"));
                return;
            }

            if (!m_reply.storage.isWritable)
            {
                QnMessageBox::warning(
                    this,
                    tr("Storage is available but will not be writable because it is too small "
                        "in comparison to the already present storages"));

                return;
            }

            m_storage = QnStorageModelInfo(m_reply.storage);
            break;
        default:
            NX_ASSERT(false);
    }

    if (storageAlreadyUsed(m_storage.url))
    {
        const auto extras =
            tr("It is not recommended to use one recording location for different servers.")
            + '\n' + tr("Add this storage anyway?");

        QnMessageBox messageBox(QnMessageBoxIcon::Warning,
            tr("Storage path used by another server"),
            extras, QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);
        messageBox.addButton(tr("Add Storage"),
            QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

        if (messageBox.exec() == QDialogButtonBox::Cancel)
            return;
    }
    base_type::accept();
}

void QnStorageUrlDialog::updateComboBox()
{
    QString lastProtocol = m_lastProtocol;

    ui->protocolComboBox->clear();
    for (const ProtocolDescription& description: m_descriptions)
    {
        ui->protocolComboBox->addItem(description.name, description.protocol);

        if (description.protocol == lastProtocol)
            ui->protocolComboBox->setCurrentIndex(ui->protocolComboBox->count() - 1);
    }

    if (ui->protocolComboBox->currentIndex() == -1 && ui->protocolComboBox->count() > 0)
        ui->protocolComboBox->setCurrentIndex(0);
}

void QnStorageUrlDialog::at_protocolComboBox_currentIndexChanged()
{
    m_urlByProtocol[m_lastProtocol] = ui->urlEdit->text().trimmed();

    int index = ui->protocolComboBox->currentIndex();
    QString url, placeholder;
    if (index == -1)
    {
        m_lastProtocol = QString();
    }
    else
    {
        m_lastProtocol = m_descriptions[index].protocol;
        placeholder = m_descriptions[index].urlTemplate;
        url = m_urlByProtocol[m_lastProtocol];
    }

    ui->urlEdit->setText(url);
    ui->urlEdit->setPlaceholderText(placeholder);
}

bool QnStorageUrlDialog::storageAlreadyUsed(const QString& path) const
{
    QnMediaServerResourceList servers = resourcePool()->getResources<QnMediaServerResource>();
    servers.removeOne(m_server);

    bool usedOnOtherServers = std::any_of(servers.begin(), servers.end(),
        [path](const QnMediaServerResourcePtr& server)
        {
            return !server->getStorageByUrl(path).isNull();
        });

    bool usedOnCurrentServer = std::any_of(m_currentServerStorages.begin(), m_currentServerStorages.end(),
        [path](const QnStorageModelInfo& info)
        {
            return info.url == path;
        });

    if (usedOnOtherServers)
        qDebug() << "storage" << path << "is used on other servers";

    if (usedOnCurrentServer)
        qDebug() << "storage" << path << "is used on current server";

    return usedOnOtherServers || usedOnCurrentServer;
}

QnStorageModelInfoList QnStorageUrlDialog::currentServerStorages() const
{
    return m_currentServerStorages;
}

void QnStorageUrlDialog::setCurrentServerStorages(const QnStorageModelInfoList& storages)
{
    m_currentServerStorages = storages;
}
