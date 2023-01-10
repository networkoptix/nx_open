// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filesystem.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <client/client_globals.h>

#include <nx/utils/log/assert.h>

#include <nx/branding.h>
#include <nx/build_info.h>

namespace nx::vms::client::desktop {

QString FileSystemStrings::suffix(FileExtension ext)
{
    switch (ext)
    {
        case FileExtension::avi:
            return "avi";
        case FileExtension::mkv:
            return "mkv";
        case FileExtension::mp4:
            return "mp4";
        case FileExtension::nov:
            return "nov";
        case FileExtension::exe:
            return "exe";
        default:
            NX_ASSERT(false, "Should never get here");
            return QString();
    }
}

FileExtension FileSystemStrings::extension(const QString& suffix, FileExtension defaultValue)
{
    if (suffix == "exe")
        return FileExtension::exe;

    if (suffix == "avi")
        return FileExtension::avi;

    if (suffix == "mp4")
        return FileExtension::mp4;

    if (suffix == "nov")
        return FileExtension::nov;

    return defaultValue;
}

QString FileSystemStrings::description(FileExtension extension)
{
    switch (extension)
    {
        case FileExtension::avi:
            return tr("Audio Video Interleave");
        case FileExtension::mkv:
            return tr("Matroska");
        case FileExtension::mp4:
            return tr("MPEG-4 Part 14");
        case FileExtension::nov:
            return tr("%1 Media File").arg(nx::branding::company());
        case FileExtension::exe:
            return tr("Executable %1 Media File").arg(nx::branding::company());

        default:
            NX_ASSERT(false, "Should never get here");
            return QString();
    }
}

QString FileSystemStrings::filterDescription(FileExtension ext)
{
    const QString formatTemplate("%1 (*.%2)");
    return formatTemplate.arg(description(ext)).arg(suffix(ext));
}

bool FileExtensionUtils::isLayout(FileExtension extension)
{
    return extension == FileExtension::nov || isExecutable(extension);
}

bool FileExtensionUtils::isExecutable(FileExtension extension)
{
    return extension == FileExtension::exe;
}

FileExtensionModel::FileExtensionModel(QObject* parent):
    base_type(parent)
{
}

FileExtensionModel::~FileExtensionModel()
{
}

FileExtensionList FileExtensionModel::extensions() const
{
    return m_data;
}

void FileExtensionModel::setExtensions(const FileExtensionList& extensions)
{
    beginResetModel();
    m_data = extensions;
    endResetModel();
}

int FileExtensionModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_data.size();
}

QVariant FileExtensionModel::data(const QModelIndex& index, int role) const
{
    const auto row = index.row();
    if (!index.isValid() || !hasIndex(row, index.column(), index.parent()))
        return QVariant();

    if (row < 0 || row >= rowCount())
        return QVariant();

    const auto extension = m_data[row];
    switch(role)
    {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleDescriptionRole:
            return FileSystemStrings::filterDescription(extension);
        case Qn::ShortTextRole:
            return QString(".%1").arg(FileSystemStrings::suffix(extension));
        case ExtensionRole:
            return QVariant::fromValue(extension);
        default:
            break;
    }
    return QVariant();
}

Filename Filename::parse(const QString& filename)
{
    QFileInfo info(filename);

    Filename result;
    result.path = info.absolutePath();
    result.name = info.completeBaseName();
    result.extension = FileSystemStrings::extension(info.suffix());
    return result;
}

QString Filename::completeFileName() const
{
    const auto ext = '.' + FileSystemStrings::suffix(extension);
    const auto fullName = name.endsWith(ext)
        ? name
        : name + ext;

    const QDir folder(path);
    return folder.absoluteFilePath(fullName);
}

bool Filename::operator==(const Filename& other) const
{
    const auto cs = nx::build_info::isWindows() ? Qt::CaseInsensitive : Qt::CaseSensitive;
    return completeFileName().compare(other.completeFileName(), cs) == 0;
}

} // namespace nx::vms::client::desktop
