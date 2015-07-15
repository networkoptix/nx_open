
#include "storage_url_dialog.h"

#include <QtWidgets/QMessageBox>

#include "ui_storage_url_dialog.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/abstract_storage_resource.h>

#include <api/media_server_connection.h>
#include "common/common_module.h"
#include "api/runtime_info_manager.h"

QnStorageUrlDialog::QnStorageUrlDialog(const QnMediaServerResourcePtr &server, QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::StorageUrlDialog()),
    m_server(server)
{
    ui->setupUi(this);

    connect(ui->protocolComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_protocolComboBox_currentIndexChanged()));
}

QnStorageUrlDialog::~QnStorageUrlDialog() {
    return;
}

QList<QString> QnStorageUrlDialog::protocols() const {
    return m_protocols;
}

void QnStorageUrlDialog::setProtocols(const QList<QString> &protocols) {
    if(m_protocols == protocols)
        return;

    m_protocols = protocols;

    m_descriptions.clear();
    for(const QString &protocol: m_protocols) {
        ProtocolDescription description = protocolDescription(protocol);
        if(!description.name.isNull())
            m_descriptions.push_back(description);
    }

    updateComboBox();
}

QnStorageSpaceData QnStorageUrlDialog::storage() const {
    return m_storage;
}

QString QnStorageUrlDialog::normalizePath(QString path) {
    QString separator = lit("/");
    ec2::ApiRuntimeData data = QnRuntimeInfoManager::instance()->item(m_server->getId()).data;
    if (data.platform.toLower() == lit("windows"))
        separator = lit("\\");
    QString result = path.replace(L'/', separator);
    result = path.replace(L'\\', separator);
    if (result.endsWith(separator))
        result.chop(1);
    return result;
}

QnStorageUrlDialog::ProtocolDescription QnStorageUrlDialog::protocolDescription(const QString &protocol) {
    ProtocolDescription result;

    QString validIpPattern = lit("(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])");
    QString validHostnamePattern = lit("(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])");

    if(protocol == lit("smb")) {
        result.protocol = protocol;
        result.name = tr("Windows Network Shared Resource");
        result.urlTemplate = tr("\\\\<Computer Name>\\<Folder>");
        result.urlPattern = lit("\\\\\\\\%1\\\\.+").arg(validHostnamePattern);
    } 
    //else if(protocol == lit("coldstore")) 
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
    if (path.indexOf(lit("://")) != -1) {
        if (!login.isEmpty()) {
            QUrl url(path);
            url.setUserName(login);
            url.setPassword(password);
            return url.toString();
        } else {
            return path;
        }
    }
    
    if (login.isEmpty()) {
        return normalizePath(path);
    }
    else {
        QUrl url = QString(lit("file:///%1")).arg(normalizePath(path));
        url.setUserName(login);
        url.setPassword(password);
        return url.toString();
    }
}

void QnStorageUrlDialog::accept() 
{
    QnConnectionRequestResult result;
    QString url = makeUrl(ui->urlEdit->text(), ui->loginLineEdit->text(), ui->passwordLineEdit->text());
    m_server->apiConnection()->getStorageStatusAsync(url,  &result, SLOT(processReply(int, const QVariant &, int)));

    QEventLoop loop;
    connect(&result, SIGNAL(replyProcessed()), &loop, SLOT(quit()));

    setEnabled(false);
    setCursor(Qt::WaitCursor);
    loop.exec();
    setEnabled(true);
    unsetCursor();

    m_storage = result.reply().value<QnStorageStatusReply>().storage;
    if(result.status() != 0 || !m_storage.isWritable || !m_storage.isExternal) {
        QMessageBox::warning(this, tr("Invalid Storage"), tr("Provided storage path does not define a valid external storage."));
        return;
    }

    base_type::accept();
}

void QnStorageUrlDialog::updateComboBox() {
    QString lastProtocol = m_lastProtocol;

    ui->protocolComboBox->clear();
    for(const ProtocolDescription &description: m_descriptions) {
        ui->protocolComboBox->addItem(description.name);

        if(description.protocol == lastProtocol)
            ui->protocolComboBox->setCurrentIndex(ui->protocolComboBox->count() - 1);
    }

    if(ui->protocolComboBox->currentIndex() == -1 && ui->protocolComboBox->count() > 0)
        ui->protocolComboBox->setCurrentIndex(0);
}

void QnStorageUrlDialog::at_protocolComboBox_currentIndexChanged() {
    m_urlByProtocol[m_lastProtocol] = ui->urlEdit->text().trimmed();
    
    int index = ui->protocolComboBox->currentIndex();
    QString url, placeholder;
    if(index == -1) {
        m_lastProtocol = QString();
    } else {
        m_lastProtocol = m_descriptions[index].protocol;
        placeholder = m_descriptions[index].urlTemplate;
        url = m_urlByProtocol[m_lastProtocol];
    }

    ui->urlEdit->setText(url);
    ui->urlEdit->setPlaceholderText(placeholder);

#ifdef Q_OS_WIN
    //ui->browseButton->setVisible(m_lastProtocol == lit("smb"));
    ui->browseButton->setVisible(false);
#else
    ui->browseButton->setVisible(false);
#endif
}
