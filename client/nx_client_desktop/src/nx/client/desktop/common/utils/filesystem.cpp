#include "filesystem.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <utils/common/app_info.h>

#include <nx/utils/log/assert.h>
#include "nx/utils/app_info.h"

namespace nx {
namespace client {
namespace desktop {

namespace {

class FilesystemStrings
{
    Q_DECLARE_TR_FUNCTIONS(FilesystemStrings)
public:
    static QString suffix(FileExtension ext)
    {
        switch (ext)
        {
            case FileExtension::avi:
                return lit("avi");
            case FileExtension::mkv:
                return lit("mkv");
            case FileExtension::mp4:
                return lit("mp4");
            case FileExtension::nov:
                return lit("nov");
            case FileExtension::exe64:
            case FileExtension::exe86:
                return lit("exe");
            default:
                NX_ASSERT(false, "Should never get here");
                return QString();
        }
    }

    static FileExtension extension(const QString& suffix)
    {
        if (suffix == lit("exe"))
        {
            if (utils::AppInfo::isWin64())
                return FileExtension::exe64;
            return FileExtension::exe86;
        }

        if (suffix == lit("avi"))
            return FileExtension::avi;

        if (suffix == lit("mp4"))
            return FileExtension::mp4;

        if (suffix == lit("nov"))
            return FileExtension::nov;

        // Default value.
        return FileExtension::mkv;
    }

    static QString description(FileExtension ext)
    {
        const QString formatTemplate(lit("%1 (*.%2)"));
        return formatTemplate.arg(descriptionInternal(ext)).arg(suffix(ext));
    }

private:
    static QString descriptionInternal(FileExtension extension)
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
                return tr("%1 Media File").arg(QnAppInfo::organizationName());
            case FileExtension::exe64:
                return tr("Executable %1 Media File (x64)").arg(QnAppInfo::organizationName());
            case FileExtension::exe86:
                return tr("Executable %1 Media File (x86)").arg(QnAppInfo::organizationName());

            default:
                NX_ASSERT(false, "Should never get here");
                return QString();
        }
    }

};

} // namespace

bool FileExtensionUtils::isExecutable(FileExtension extension)
{
    return extension == FileExtension::exe64 || extension == FileExtension::exe86;
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

int FileExtensionModel::rowCount(const QModelIndex& parent) const
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
            return FilesystemStrings::description(extension);
        case Qn::ShortTextRole:
            return lit(".%1").arg(FilesystemStrings::suffix(extension));
        case ExtensionRole:
            return qVariantFromValue(extension);
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
    result.extension = FilesystemStrings::extension(info.suffix());
    return result;
}

QString Filename::completeFileName() const
{
    const auto ext = L'.' + FilesystemStrings::suffix(extension);
    const auto fullName = name.endsWith(ext)
        ? name
        : name + ext;

    const QDir folder(path);
    return folder.absoluteFilePath(fullName);
}

bool Filename::operator==(const Filename& other) const
{
    const auto cs = utils::AppInfo::isWindows() ? Qt::CaseInsensitive : Qt::CaseSensitive;
    return completeFileName().compare(other.completeFileName(), cs) == 0;
}

} // namespace desktop
} // namespace client
} // namespace nx
