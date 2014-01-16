#include "two_step_file_dialog.h"
#include "ui_two_step_file_dialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

#include <ui/style/warning_style.h>

#include <utils/common/string.h>

namespace {

    /** Makes a list of filters from ;;-separated text (qfiledialog.cpp). */
    static QStringList qt_make_filter_list(const QString &filter) {
        QString f(filter);

        if (f.isEmpty())
            return QStringList();

        QString sep(QLatin1String(";;"));
        int i = f.indexOf(sep, 0);
        if (i == -1) {
            if (f.indexOf(QLatin1Char('\n'), 0) != -1) {
                sep = QLatin1Char('\n');
                i = f.indexOf(sep, 0);
            }
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
    ui(new Ui::QnTwoStepFileDialog)
{
    ui->setupUi(this);
    setWindowTitle(caption);

    ui->directoryLabel->setText(workingDirectory(initialFile));
    ui->fileNameLineEdit->setText(initialSelection(initialFile));

    setNameFilters(qt_make_filter_list(filter));
    setWarningStyle(ui->alreadyExistsLabel);

    connect(ui->fileNameLineEdit,   &QLineEdit::textChanged,    this,   &QnTwoStepFileDialog::updateFileExistsWarning);
    connect(ui->browseButton,       &QPushButton::clicked,      this,   &QnTwoStepFileDialog::at_browseButton_clicked);
    connect(ui->filterComboBox, SIGNAL(QComboBox::currentIndexChanged(int)), this, SLOT(QnTwoStepFileDialog::updateFileExistsWarning)); //f**ing overloaded currentIndexChanged

    updateFileExistsWarning();
}

QnTwoStepFileDialog::~QnTwoStepFileDialog() { }

void QnTwoStepFileDialog::setOptions(QFileDialog::Options options) {
    Q_ASSERT(options == 0);
}

void QnTwoStepFileDialog::setFileMode(QFileDialog::FileMode mode) {
    Q_ASSERT(mode == QFileDialog::AnyFile);
}

void QnTwoStepFileDialog::setAcceptMode(QFileDialog::AcceptMode mode) {
    Q_ASSERT(mode == QFileDialog::AcceptSave);
}

QString QnTwoStepFileDialog::selectedFile() const {
    QFileInfo info(QDir(ui->directoryLabel->text()), ui->fileNameLineEdit->text());
    QString fileName = info.absoluteFilePath();

    QString selectedExtension = extractFileExtension(selectedNameFilter());

    if (!fileName.endsWith(selectedExtension))
        fileName += selectedExtension;
    return fileName;
}

QString QnTwoStepFileDialog::selectedNameFilter() const {
    return ui->filterComboBox->currentText();
}

QGridLayout* QnTwoStepFileDialog::customizedLayout() const {
    return ui->optionsLayout;
}

void QnTwoStepFileDialog::setNameFilters(const QStringList &filters) {
    ui->filterComboBox->clear();
    ui->filterComboBox->addItems(filters);
}

void QnTwoStepFileDialog::updateFileExistsWarning() {
    ui->alreadyExistsLabel->setVisible(QFileInfo(selectedFile()).exists());
}

void QnTwoStepFileDialog::at_browseButton_clicked() {
    QString dirName = QFileDialog::getExistingDirectory(this,
                                                        tr("Select directory"),
                                                        ui->directoryLabel->text(),
                                                        directoryDialogOptions());
    if (!dirName.isEmpty())
        ui->directoryLabel->setText(dirName);
}
