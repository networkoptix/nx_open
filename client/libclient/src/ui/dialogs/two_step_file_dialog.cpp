#include "two_step_file_dialog.h"
#include "ui_two_step_file_dialog.h"

#include <QtCore/QTimer>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

#include <ui/style/custom_style.h>
#include <ui/dialogs/common/file_dialog.h>

#include <nx/utils/string.h>

namespace {

    /** Makes a list of filters from ;;-separated text (qfiledialog.cpp). */
    static QStringList qt_make_filter_list(const QString &filter) {
        QString f(filter);

        if (f.isEmpty())
            return QStringList();

        QString sep(QLatin1String(";;"));
        int i = f.indexOf(sep, 0);
        if (i == -1) {
            if (f.indexOf(QLatin1Char('\n'), 0) != -1)
                sep = QLatin1Char('\n');
        }

        return f.split(sep);
    }

    inline static QString _qt_get_directory(const QString &path)
    {
        QFileInfo info = QFileInfo(QDir::current(), path);
        if (info.exists() && info.isDir())
            return QDir::cleanPath(info.absoluteFilePath());
        info.setFile(info.absolutePath());
        if (info.exists() && info.isDir())
            return info.absoluteFilePath();
        return QString();
    }

    static QString workingDirectory(const QString &path) {
        if (!path.isEmpty()) {
            QString directory = _qt_get_directory(path);
            if (!directory.isEmpty())
                return directory;
        }
        return QDir::currentPath();
    }

    static QString initialSelection(const QString &path) {
        if (!path.isEmpty()) {
            QFileInfo info(path);
            if (!info.isDir())
                return info.fileName();
        }
        return QString();
    }

}

QnTwoStepFileDialog::QnTwoStepFileDialog(QWidget *parent, const QString &caption, const QString &initialFile, const QString &filter) :
    base_type(parent),
    ui(new Ui::QnTwoStepFileDialog),
    m_mode(QFileDialog::AnyFile),
    m_filter(filter)
{
    ui->setupUi(this);
    setWindowTitle(caption);

    ui->directoryLabel->setText(workingDirectory(initialFile));
    ui->fileNameLineEdit->setText(initialSelection(initialFile));

    setNameFilters(qt_make_filter_list(filter));
    setWarningStyle(ui->alreadyExistsLabel);

    connect(ui->fileNameLineEdit,   &QLineEdit::textChanged,    this,   &QnTwoStepFileDialog::updateFileExistsWarning);
    connect(ui->browseFolderButton, &QPushButton::clicked,      this,   &QnTwoStepFileDialog::at_browseFolderButton_clicked);
    connect(ui->browseFileButton,   &QPushButton::clicked,      this,   &QnTwoStepFileDialog::at_browseFileButton_clicked);
    connect(ui->filterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFileExistsWarning())); //f**ing overloaded currentIndexChanged

    updateMode();
    updateFileExistsWarning();
}

QnTwoStepFileDialog::~QnTwoStepFileDialog() { }

void QnTwoStepFileDialog::setOptions(QFileDialog::Options options) {
    NX_ASSERT(options == 0);
}

void QnTwoStepFileDialog::setFileMode(QFileDialog::FileMode mode) {
    if (mode == m_mode)
        return;
    m_mode = mode;
    updateMode();
}

void QnTwoStepFileDialog::setAcceptMode(QFileDialog::AcceptMode mode) {
    NX_ASSERT(mode == QFileDialog::AcceptSave);
}

QString QnTwoStepFileDialog::selectedFile() const {
    QString fileName;

    switch (m_mode) {
    case QFileDialog::AnyFile: {
        if (ui->fileNameLineEdit->text().isEmpty())
            return QString();

        QFileInfo info(QDir(ui->directoryLabel->text()), ui->fileNameLineEdit->text());
        fileName = info.absoluteFilePath();
        break;
    }
    case QFileDialog::ExistingFile: {
        if (ui->existingFileLabel->text().isEmpty())
            return QString();

        fileName = ui->existingFileLabel->text();
        break;
    }
    default:
        NX_ASSERT(false);
    }

    QString selectedExtension = nx::utils::extractFileExtension(selectedNameFilter());

    if (!fileName.endsWith(selectedExtension))
        fileName += selectedExtension;
    return fileName;
}

QString QnTwoStepFileDialog::selectedNameFilter() const {
    switch (m_mode) {
    case QFileDialog::AnyFile:
        return ui->filterComboBox->currentText();
    case QFileDialog::ExistingFile:
        return m_selectedExistingFilter;
    default:
        break;
    }
    NX_ASSERT(false);
    return QString();
}

int QnTwoStepFileDialog::exec() {
    ui->optionsGroupBox->setVisible(!ui->optionsLayout->isEmpty());
    return base_type::exec();
}

bool QnTwoStepFileDialog::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Polish && m_mode == QFileDialog::ExistingFile)
        at_browseFileButton_clicked();

    return result;
}

QGridLayout* QnTwoStepFileDialog::customizedLayout() const {
    return ui->optionsLayout;
}

void QnTwoStepFileDialog::setNameFilters(const QStringList &filters) {
    ui->filterComboBox->clear();
    ui->filterComboBox->addItems(filters);
}

void QnTwoStepFileDialog::updateFileExistsWarning() {
    ui->alreadyExistsLabel->setVisible(m_mode != QFileDialog::ExistingFile
            && QFileInfo(selectedFile()).exists());
}

void QnTwoStepFileDialog::updateMode() {
    NX_ASSERT(m_mode == QFileDialog::ExistingFile || m_mode == QFileDialog::AnyFile);

    ui->existingFileWidget->setVisible(m_mode == QFileDialog::ExistingFile);
    ui->newFileWidget->setVisible(m_mode == QFileDialog::AnyFile);
}

void QnTwoStepFileDialog::at_browseFolderButton_clicked() {
    QString dirName = QnFileDialog::getExistingDirectory(this,
                                                        tr("Select folder..."),
                                                        ui->directoryLabel->text(),
                                                        directoryDialogOptions());
    if (dirName.isEmpty())
        return;

    ui->directoryLabel->setText(dirName);
}

void QnTwoStepFileDialog::at_browseFileButton_clicked() {
    QString fileName = QnFileDialog::getOpenFileName(this,
                                                    tr("Select file..."),
                                                    ui->existingFileLabel->text(),
                                                    m_filter,
                                                    &m_selectedExistingFilter,
                                                    fileDialogOptions());
    if (fileName.isEmpty()) {
        if (ui->existingFileLabel->text().isEmpty())
            QTimer::singleShot(1, this, SLOT(reject()));
//            accept();   //close dialog immediately
        return;
    }

    ui->existingFileLabel->setText(fileName);
}
