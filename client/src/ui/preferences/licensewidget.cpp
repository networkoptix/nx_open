#include "licensewidget.h"
#include "ui_licensewidget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "licensing/license.h"

LicenseWidget::LicenseWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LicenseWidget),
    m_httpClient(0)
{
    ui->setupUi(this);

    ui->serialKeyEdit->setInputMask(QLatin1String(">NNNN-NNNN-NNNN-NNNN"));

    connect(ui->licenseGoupBox, SIGNAL(toggled(bool)), this, SLOT(setOnlineActivation(bool)));
    connect(ui->browseLicenseFileButton, SIGNAL(clicked()), this, SLOT(browseLicenseFileButtonClicked()));
    connect(ui->licenseDetailsButton, SIGNAL(clicked()), this, SLOT(licenseDetailsButtonClicked()));
    connect(ui->activateLicenseButton, SIGNAL(clicked()), this, SLOT(activateLicenseButtonClicked()));

    setOnlineActivation(ui->licenseGoupBox->isChecked());

    ui->hardwareIdEdit->setText(qnLicensePool->hardwareId());

    ui->serialKeyEdit->setFocus();
}

LicenseWidget::~LicenseWidget()
{
}

void LicenseWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        //retranslateUi();
        break;
    default:
        break;
    }
}

void LicenseWidget::setOnlineActivation(bool online)
{
    ui->manualActivationInfoLabel->setVisible(!online);
    ui->serialKeyEdit->setReadOnly(!online);
    ui->hardwareIdLabel->setVisible(!online);
    ui->hardwareIdEdit->setVisible(!online);
    ui->activationKeyLabel->setVisible(!online);
    ui->activationKeyTextEdit->setVisible(!online);
    ui->browseLicenseFileButton->setVisible(!online);

    // re-enable child widgets after group box'es check button was taggled
    foreach (QObject *o, ui->licenseGoupBox->children()) {
        if (o->isWidgetType()) {
            QWidget *w = static_cast<QWidget *>(o);
            if (!w->testAttribute(Qt::WA_ForceDisabled))
                w->setEnabled(true);
        }
    }

    updateControls();
}

void LicenseWidget::browseLicenseFileButtonClicked()
{
    QFileDialog dialog(this, tr("Open License file"));
    dialog.setNameFilters(QStringList() << tr("All files (*.*)"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec() && !dialog.selectedFiles().isEmpty()) {
        QFile file(dialog.selectedFiles().first());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            ui->activationKeyTextEdit->setPlainText(QString::fromLatin1(file.readAll()));
    }
}

void LicenseWidget::licenseDetailsButtonClicked()
{
    // get tab element
    const QnLicense license("", "", 1, "", ""); // = QnLicense::defaultLicense();

    QString details = tr("<b>Generic:</b><br />\n"
                         "License Qwner: %1<br />\n"
                         "Serial Key: %2<br />\n"
                         "Locked to Hardware ID: %3<br />\n"
                         "<br />\n"
                         "<b>Features:</b><br />\n"
                         "Archive Streams Allowed: %4")
                      .arg(license.name().constData())
                      .arg(license.key().constData())
                      .arg(license.hardwareId().constData())
                      .arg(license.cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

void LicenseWidget::activateLicenseButtonClicked()
{
    ui->licenseDetailsButton->setEnabled(false);

    ui->activateLicenseButton->setEnabled(false);
    ui->activateLicenseButton->setText(tr("Activating..."));

    if (ui->licenseGoupBox->isChecked())
        updateFromServer(ui->serialKeyEdit->text(), ui->hardwareIdEdit->text());
    else
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(ui->activationKeyTextEdit->toPlainText().toLatin1()))));
}

void LicenseWidget::updateFromServer(const QString &serialKey, const QString &hardwareId)
{
    if (!m_httpClient)
        m_httpClient = new QNetworkAccessManager(this);

    QUrl url(QLatin1String("http://noptix.enk.me/~ivan_vigasin/nolicensed/activate.php"));

    QNetworkRequest request;
    request.setUrl(url);

    QUrl params;
    params.addQueryItem(QLatin1String("license_key"), serialKey);
    params.addQueryItem(QLatin1String("hwid"), hardwareId);

    QNetworkReply *reply = m_httpClient->post(request, params.encodedQuery());

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(downloadError()));
    connect(reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void LicenseWidget::downloadError()
{
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        disconnect(reply, SIGNAL(finished()), this, SLOT(downloadFinished()));

        ui->licenseGoupBox->setChecked(false);

        QMessageBox::warning(this, tr("License Activation"),
                             tr("Network error has ocurred during the Automatic License Activation.\nTry activate your License manually."));

        reply->deleteLater();
    }
}

void LicenseWidget::downloadFinished()
{
    if (QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender())) {
        validateLicense(QnLicensePtr(new QnLicense(QnLicense::fromString(reply->readAll()))));

        reply->deleteLater();
    }
}

void LicenseWidget::updateControls()
{
    if (qnLicensePool->haveValidLicense()) {
        ui->licenseDetailsButton->setEnabled(true);
    }
/*
    if (license.isValid()) {
        ui->serialKeyEdit->setText(license.serialKey());
        ui->activationKeyTextEdit->setPlainText(QString::fromLatin1(license.toString()));

        ui->licenseDetailsButton->setEnabled(true);
    }
*/
    ui->activateLicenseButton->setText(tr("Activate License"));
    ui->activateLicenseButton->setEnabled(true);
}

void LicenseWidget::validateLicense(const QnLicensePtr &license)
{
    if (license->isValid()) {
        qnLicensePool->addLicense(license);

        updateControls();

        QMessageBox::information(this, tr("License Activation"),
                                 tr("License was succesfully activated."));
    } else {
        QMessageBox::warning(this, tr("License Activation"),
                             tr("Invalid License. Contact our support team to get a valid License."));
    }
}
