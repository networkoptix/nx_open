#include "storage_url_dialog.h"
#include "ui_storage_url_dialog.h"

#include <api/media_server_connection.h>
#include <api/runtime_info_manager.h>

#include "common/common_module.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/abstract_storage_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/style/custom_style.h>
#include <ui/widgets/common/busy_indicator_button.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <ui/common/scoped_cursor_rollback.h>
#include <utils/common/scoped_value_rollback.h>


QnStorageUrlDialog::QnStorageUrlDialog(
        const QnMediaServerResourcePtr& server,
        QWidget* parent,
        Qt::WindowFlags windowFlags) :

    base_type(parent, windowFlags),
    ui(new Ui::StorageUrlDialog()),
    m_server(server),
    m_protocols(),
    m_descriptions(),
    m_urlByProtocol(),
    m_lastProtocol(),
    m_storage(),
    m_currentServerStorages(),
    m_okButton(new QnBusyIndicatorButton(this))
{
    ui->setupUi(this);
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

    QStringList sortedProtocols(m_protocols.toList());
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

QString QnStorageUrlDialog::normalizePath(QString path)
{
    QString separator = lit("/");
    //ec2::ApiRuntimeData data = QnRuntimeInfoManager::instance()->item(m_server->getId()).data;
    //if (data.platform.toLower() == lit("windows"))
    //    separator = lit("\\");
    QString result = path.replace(L'/', separator);
    result = path.replace(L'\\', separator);
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

    if (urlString.indexOf(lit("://")) == -1)
        urlString = lit("smb://%1").arg(urlString);
    QUrl url(urlString);
    if (!login.isEmpty())
        url.setUserName(login);
    if (!password.isEmpty())
        url.setPassword(password);
    return url.toString();
}

void QnStorageUrlDialog::accept()
{
    QnConnectionRequestResult result;
    QString protocol = qvariant_cast<QString>(ui->protocolComboBox->currentData());
    QString urlText = ui->urlEdit->text();

    if (protocol == lit("smb"))
    {
        if (!urlText.startsWith(lit("\\\\")))
            urlText = lit("\\\\") + urlText;
    }
    else if (!urlText.toUpper().startsWith(protocol.toUpper() + lit("://")))
    {
        urlText = protocol.toLower() + lit("://") + urlText;
    }

    QString url = makeUrl(urlText, ui->loginLineEdit->text(), ui->passwordLineEdit->text());
    m_server->apiConnection()->getStorageStatusAsync(url,  &result, SLOT(processReply(int, const QVariant &, int)));

    enum {
        kFinished,
        kCancelled
    };

    QEventLoop loop;
    connect(&result, &QnConnectionRequestResult::replyProcessed, &loop,
        [&loop]() { loop.exit(kFinished); });
    connect(this, &QDialog::rejected, &loop,
        [&loop]() { loop.exit(kCancelled); });

    // Scoped event loop:
    {
        QnScopedCursorRollback cursorRollback(this, Qt::WaitCursor);
        QnScopedTypedPropertyRollback<bool, QWidget> inputsEnabledRollback(ui->inputsWidget,
            &QWidget::setEnabled, &QWidget::isEnabled, false);
        QnScopedTypedPropertyRollback<bool, QWidget> buttonEnabledRollback(m_okButton,
            &QWidget::setEnabled, &QWidget::isEnabled, false);
        QnScopedTypedPropertyRollback<bool, QnBusyIndicatorButton> buttonIndicatorRollback(m_okButton,
            &QnBusyIndicatorButton::showIndicator, &QnBusyIndicatorButton::isIndicatorVisible, true);

        if (loop.exec() == kCancelled)
            return;
    }

    m_storage = QnStorageModelInfo(result.reply().value<QnStorageStatusReply>().storage);
    Qn::StorageInitResult initStatus = result.reply().value<QnStorageStatusReply>().status;

    if (result.status() == 0 && initStatus == Qn::StorageInit_WrongAuth)
    {
        QnMessageBox::warning(this, tr("Invalid Storage"), tr("Invalid external storage credentials."));
        return;
    }

    if (!(result.status() == 0
            && initStatus == Qn::StorageInit_Ok
            && m_storage.isWritable
            && m_storage.isExternal))
    {
        QnMessageBox::warning(this, tr("Invalid Storage"), tr("Provided storage path does not point to a valid external storage location."));
        return;
    }

    if (storageAlreadyUsed(m_storage.url))
    {
        QString message = tr("System has other server(s) using the same network storage path. "\
                             "Recording data by multiple servers to exactly same place is not recommended.");

        QnMessageBox messageBox(QnMessageBox::Warning, 0, tr("Warning!"), message, QDialogButtonBox::Cancel);
        messageBox.addButton(tr("Add storage"), QDialogButtonBox::AcceptRole);

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
    QnMediaServerResourceList servers = qnResPool->getResources<QnMediaServerResource>();
    servers.removeOne(m_server);

    bool usedOnOtherServers = boost::algorithm::any_of(servers,
        [path](const QnMediaServerResourcePtr& server)
        {
            return !server->getStorageByUrl(path).isNull();
        });

    bool usedOnCurrentServer = boost::algorithm::any_of(m_currentServerStorages,
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
