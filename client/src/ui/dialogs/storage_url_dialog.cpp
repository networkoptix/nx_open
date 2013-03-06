#include "storage_url_dialog.h"
#include "ui_storage_url_dialog.h"

#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>

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
    foreach(const QString &protocol, m_protocols) {
        QnAbstractStorageResource::ProtocolDescription description = QnAbstractStorageResource::protocolDescription(protocol);
        if(!description.name.isNull())
            m_descriptions.push_back(description);
    }

    updateComboBox();
}

QnStorageSpaceData QnStorageUrlDialog::storage() const {
    return m_storage;
}

void QnStorageUrlDialog::accept() {
    QnMediaServerRequestResult result;
    m_server->apiConnection()->asyncGetStorageStatus(ui->urlEdit->text(), &result, SLOT(processReply(int, const QVariant &, int)));

    QEventLoop loop;
    connect(&result, SIGNAL(replyProcessed()), &loop, SLOT(quit()));

    setEnabled(false);
    setCursor(Qt::WaitCursor);
    loop.exec();
    setEnabled(true);
    unsetCursor();

    m_storage = result.reply().value<QnStorageStatusReply>().storage;
    if(result.status() != 0 || !m_storage.isWritable) {
        QMessageBox::warning(this, tr("Invalid Storage"), tr("Provided storage path is not valid."));
        return;
    }

    base_type::accept();
}

void QnStorageUrlDialog::updateComboBox() {
    QString lastProtocol = m_lastProtocol;

    ui->protocolComboBox->clear();
    foreach(QnAbstractStorageResource::ProtocolDescription description, m_descriptions) {
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

