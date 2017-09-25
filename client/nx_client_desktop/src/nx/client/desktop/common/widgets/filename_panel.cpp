#include "filename_panel.h"
#include "ui_filename_panel.h"

#include <client/client_settings.h>

#include <ui/common/aligner.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

namespace {

static constexpr int kFilterComboBoxWidth = 120;

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct FilenamePanel::Private
{
    Filename filename;
    FileExtensionModel model;
};

FilenamePanel::FilenamePanel(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilenamePanel),
    d(new Private)
{
    ui->setupUi(this);
    ui->extensionsComboBox->setModel(&d->model);
    ui->extensionsComboBox->setMaximumWidth(kFilterComboBoxWidth);
    ui->extensionsComboBox->setCustomTextRole(Qn::ShortTextRole);

    const auto aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({ui->folderInputField, ui->nameInputField});

    ui->folderInputField->setTitle(tr("Folder"));
    ui->folderInputField->setReadOnly(true);
    ui->nameInputField->setTitle(tr("Name"));
    ui->nameInputField->setValidator(Qn::defaultNonEmptyValidator(tr("Name cannot be empty.")));

    connect(this, &FilenamePanel::filenameChanged, this,
        [this](const Filename& filename)
        {
            ui->nameInputField->setToolTip(filename.completeFileName());
        });

    connect(ui->nameInputField, &QnInputField::textChanged, this,
        [this](const QString& text)
        {
            d->filename.name = text;
            emit filenameChanged(filename());
        });

    connect(ui->browsePushButton, &QPushButton::clicked, this,
        [this]
        {
            const auto folder = QnFileDialog::getExistingDirectory(this,
                tr("Select folder..."),
                d->filename.path,
                QFileDialog::ShowDirsOnly);

            // Workaround for bug QTBUG-34767
            if (nx::utils::AppInfo::isMacOsX())
                raise();

            if (folder.isEmpty())
                return;

            d->filename.path = folder;
            ui->folderInputField->setText(folder);
            emit filenameChanged(filename());
        });

    connect(ui->extensionsComboBox, QnComboboxCurrentIndexChanged, this,
        [this](int /*row*/)
        {
            d->filename.extension = ui->extensionsComboBox->currentData(
                FileExtensionModel::Role::ExtensionRole).value<FileExtension>();
            emit filenameChanged(filename());
        });

}

FilenamePanel::~FilenamePanel()
{
}

FileExtensionList FilenamePanel::allowedExtesions() const
{
    return d->model.extensions();
}

void FilenamePanel::setAllowedExtensions(const FileExtensionList& extensions)
{
    d->model.setExtensions(extensions);
    ui->extensionsComboBox->setEnabled(!extensions.empty());
    updateExtension();

    emit filenameChanged(filename());
}

Filename FilenamePanel::filename() const
{
    return d->filename;
}

void FilenamePanel::setFilename(const Filename& value)
{
    d->filename = value;
    if (d->filename.path.isEmpty())
        d->filename.path = qnSettings->lastExportDir();
    if (d->filename.path.isEmpty())
        d->filename.path = qnSettings->mediaFolder();

    ui->folderInputField->setText(d->filename.path);

    ui->nameInputField->setText(d->filename.name);
    updateExtension();

    emit filenameChanged(filename());
}

void FilenamePanel::updateExtension()
{
    if (d->model.extensions().empty())
        return;

    const auto index = d->model.extensions().indexOf(d->filename.extension);
    if (index < 0)
    {
        ui->extensionsComboBox->setCurrentIndex(0);
        d->filename.extension = ui->extensionsComboBox->currentData(
            FileExtensionModel::Role::ExtensionRole).value<FileExtension>();
    }
    else
    {
        ui->extensionsComboBox->setCurrentIndex(index);
    }
}

bool FilenamePanel::validate()
{
    return ui->nameInputField->validate();
}

} // namespace desktop
} // namespace client
} // namespace nx
