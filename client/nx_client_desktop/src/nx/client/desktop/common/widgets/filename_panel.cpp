#include "filename_panel.h"
#include "ui_filename_panel.h"

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>

#include <client/client_settings.h>

#include <ui/dialogs/common/file_dialog.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/utils/app_info.h>
#include <nx/utils/algorithm/index_of.h>
#include <nx/utils/log/log.h>

namespace {

// TODO: #GDM make better place for these methods, together with constructing one.
QString getDirectory(const QString& path)
{
    QFileInfo info = QFileInfo(QDir::current(), path);
    if (info.exists() && info.isDir())
        return QDir::cleanPath(info.absoluteFilePath());
    info.setFile(info.absolutePath());
    if (info.exists() && info.isDir())
        return info.absoluteFilePath();
    return QString();
}

QString getFilename(const QString& path)
{
    if (!path.isEmpty())
    {
        QFileInfo info(path);
        if (!info.isDir())
            return info.fileName();
    }
    return QString();
}

QString getExtension(const QString& path)
{
    QRegularExpression re(QLatin1String(".*(\\.\\w+)[^\\.]*"));
    const auto match = re.match(path);
    NX_ASSERT(match.hasMatch());
    if (match.hasMatch())
        return match.captured(1);

    return QString();
}

}

namespace nx {
namespace client {
namespace desktop {

struct FilenamePanel::Private
{
    QString folder;
    QString name;
    QString extension; //< Extension of the result file (including dot).
    QStringList allowedExtesions;
};

FilenamePanel::FilenamePanel(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilenamePanel),
    d(new Private)
{
    ui->setupUi(this);

    connect(this, &FilenamePanel::filenameChanged, ui->filenameLineEdit, &QWidget::setToolTip);

    connect(ui->filenameLineEdit, &QLineEdit::textChanged, this,
        [this](const QString& text)
        {
            d->name = text;
            emit filenameChanged(filename());
        });

    connect(ui->browsePushButton, &QPushButton::clicked, this,
        [this]
        {
            const auto folder = QnFileDialog::getExistingDirectory(this,
                tr("Select folder..."),
                d->folder,
                QFileDialog::ShowDirsOnly);

            // Workaround for bug QTBUG-34767
            if (nx::utils::AppInfo::isMacOsX())
                raise();

            if (folder.isEmpty())
                return;

            d->folder = folder;
            ui->folderLineEdit->setText(folder);
            emit filenameChanged(filename());
        });

    connect(ui->extensionsComboBox, &QComboBox::currentTextChanged, this,
        [this](const QString& text)
        {
            d->extension = getExtension(text);
            emit filenameChanged(filename());
        });

}

FilenamePanel::~FilenamePanel()
{
}

QStringList FilenamePanel::allowedExtesions() const
{
    return d->allowedExtesions;
}

void FilenamePanel::setAllowedExtensions(const QStringList& extensions)
{
    d->allowedExtesions = extensions;

    ui->extensionsComboBox->clear();
    ui->extensionsComboBox->addItems(extensions);

    ui->extensionsComboBox->setEnabled(!extensions.empty());
    if (extensions.empty())
    {
        d->extension = QString();
        return;
    }

    updateExtension();

    emit filenameChanged(filename());
}

QString FilenamePanel::filename() const
{
    QDir folder(d->folder);
    return folder.absoluteFilePath(d->name + d->extension);
}

void FilenamePanel::setFilename(const QString& value)
{
    d->folder = getDirectory(value);
    if (d->folder.isEmpty())
        d->folder = qnSettings->lastExportDir();
    if (d->folder.isEmpty())
        d->folder = qnSettings->mediaFolder();

    ui->folderLineEdit->setText(d->folder);

    d->name = getFilename(value);
    ui->filenameLineEdit->setText(d->name);

    d->extension = getExtension(value);
    updateExtension();

    emit filenameChanged(filename());
}

void FilenamePanel::updateExtension()
{
    int selectedIndex = utils::algorithm::index_of(d->allowedExtesions,
        [ext = d->extension](const QString& elem) { return elem.contains(ext); });

    if (selectedIndex < 0)
    {
        ui->extensionsComboBox->setCurrentIndex(0);
        d->extension = d->allowedExtesions.first();
    }
    else
    {
        ui->extensionsComboBox->setCurrentIndex(selectedIndex);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
