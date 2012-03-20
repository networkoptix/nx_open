#include "youtubeuploaddialog.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include <utils/common/warnings.h>

#include <ui/workbench/workbench_context.h>
#define USE_PREFERENCESWND
#ifdef USE_PREFERENCESWND
#include <ui/preferences/preferencesdialog.h>
#endif

#include "youtube_uploader.h"
#include "youtubesettingswidget.h"
#include "youtubeuploadprogressdialog_p.h"
#include "ui_youtubeuploaddialog.h"

YouTubeUploadDialog::YouTubeUploadDialog(QnWorkbenchContext *context, QnResourcePtr resource, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::YouTubeUploadDialog),
    m_context(context)
{
    if(!context)
        qnNullWarning(context);

    ui->setupUi(this);

    connect(ui->browseButton, SIGNAL(clicked()), this, SLOT(browseFile()));

    if (resource) {
        ui->filePathLabel->setText(resource->getUniqueId());
        ui->titleLineEdit->setText(resource->getName());
        ui->tagsLineEdit->setText(resource->tagList().join(QLatin1String(", ")).toUpper());
    }

    // ### unused for now
    delete ui->licanseLabel;
    delete ui->licenseGroupBox;
    adjustSize();
    // ###

    ui->buttonBox->button(QDialogButtonBox::Save)->setText(tr("Upload"));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

    ui->titleLineEdit->setFocus();


    m_youtubeuploader = new YouTubeUploader(this);
    m_youtubeuploader->setLogin(YouTubeSettingsWidget::login());
    m_youtubeuploader->setPassword(YouTubeSettingsWidget::password());

    connect(m_youtubeuploader, SIGNAL(categoryListLoaded()), this, SLOT(categoryListLoaded()));
    connect(m_youtubeuploader, SIGNAL(authFailed()), this, SLOT(authFailed()));
    connect(m_youtubeuploader, SIGNAL(authFinished()), this, SLOT(authFinished()));

    retranslateUi();
}

YouTubeUploadDialog::~YouTubeUploadDialog()
{
    delete ui;
}

void YouTubeUploadDialog::accept()
{
    if (!ui->buttonBox->button(QDialogButtonBox::Save)->isEnabled())
        return;

    QString filename = ui->filePathLabel->text();
    QString title = ui->titleLineEdit->text();
    if (!QFile::exists(filename) || title.isEmpty()) {
        // ### validate data and handle errors
        return;
    }

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

    QString description = ui->descriptionTextEdit->toPlainText();

    QStringList tags;
    foreach (QString tag, ui->tagsLineEdit->text().split(QLatin1Char(','), QString::SkipEmptyParts)) {
        tag = tag.trimmed();
        if (!tag.isEmpty())
            tags.append(tag);
    }

    QString category = ui->categoryComboBox->itemData(ui->categoryComboBox->currentIndex()).toString();

    YouTubeUploader::Privacy privacy = YouTubeUploader::Privacy_Public;
    if (ui->privacyUnlistedRadioButton->isChecked())
        privacy = YouTubeUploader::Privacy_Unlisted;
    else if (ui->privacyPrivateRadioButton->isChecked())
        privacy = YouTubeUploader::Privacy_Private;

    m_youtubeuploader->uploadFile(filename, title, description, tags, category, privacy);

    // not yet
    //QDialog::accept();
}

void YouTubeUploadDialog::retranslateUi()
{
    setWindowTitle(tr("Upload File to YouTube"));
    // ### retranslate categories list
}

void YouTubeUploadDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        retranslateUi();
        break;
    default:
        break;
    }
}

void YouTubeUploadDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
        return;

    QDialog::keyPressEvent(e);
}

void YouTubeUploadDialog::browseFile()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setDirectory(ui->filePathLabel->text());
    if (dialog.exec())
        ui->filePathLabel->setText(QDir::toNativeSeparators(dialog.selectedFiles().first()));
}

void YouTubeUploadDialog::categoryListLoaded()
{
    typedef QPair<QString, QString> StringPair;
    foreach (const StringPair &pair, m_youtubeuploader->getCategoryList())
        ui->categoryComboBox->addItem(pair.second, pair.first);

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);
}

void YouTubeUploadDialog::authFailed()
{
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);

#ifndef USE_PREFERENCESWND
    QDialog dialog(this);

    YouTubeSettingsWidget *widget = new YouTubeSettingsWidget(&dialog);
    widget->layout()->setContentsMargins(0, 0, 0, 0);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, Qt::Horizontal, &dialog);
    connect(buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), &dialog, SLOT(accept()));
    connect(buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), &dialog, SLOT(reject()));

    QVBoxLayout *dialogLayout = new QVBoxLayout;
    dialogLayout->addWidget(widget);
    dialogLayout->addWidget(buttonBox);
    dialog.setLayout(dialogLayout);
#else
    QnPreferencesDialog dialog(m_context.data(), this);
    dialog.setCurrentPage(QnPreferencesDialog::PageYouTubeSettings);
    dialog.exec();
#endif

    if (dialog.exec() == QDialog::Accepted) {
#ifndef USE_PREFERENCESWND
        widget->accept();
#endif

        m_youtubeuploader->setLogin(YouTubeSettingsWidget::login());
        m_youtubeuploader->setPassword(YouTubeSettingsWidget::password());

        accept(); // retry
    }
}

void YouTubeUploadDialog::authFinished()
{
    disconnect(m_youtubeuploader, SIGNAL(authFailed()), this, SLOT(authFailed()));
    disconnect(m_youtubeuploader, SIGNAL(authFinished()), this, SLOT(authFinished()));

    YouTubeUploadProgressDialog *progressDialog = new YouTubeUploadProgressDialog;
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    progressDialog->setAttribute(Qt::WA_QuitOnClose);
    progressDialog->setWindowFlags(Qt::WindowStaysOnTopHint);
    progressDialog->setWindowTitle(tr("Uploading to YouTube"));
    progressDialog->setLabelText(tr("Uploading \"%1\"...").arg(m_youtubeuploader->fileTitle()));

    m_youtubeuploader->setParent(progressDialog);

    connect(m_youtubeuploader, SIGNAL(uploadProgress(qint64,qint64)), progressDialog, SLOT(uploadProgress(qint64,qint64)), Qt::QueuedConnection);
    connect(m_youtubeuploader, SIGNAL(uploadFailed(QString,int)), progressDialog, SLOT(uploadFailed(QString,int)), Qt::QueuedConnection);
    connect(m_youtubeuploader, SIGNAL(uploadFinished()), progressDialog, SLOT(uploadFinished()), Qt::QueuedConnection);

    progressDialog->open();

    QDialog::accept();
}
