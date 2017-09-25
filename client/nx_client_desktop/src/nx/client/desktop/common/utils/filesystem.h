#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QAbstractListModel>

namespace nx {
namespace client {
namespace desktop {

/** List of supported file extensions, used in the client. */
enum class FileExtension
{
    avi,
    mkv,
    mp4,
    nov,
    exe64,
    exe86,
};
using FileExtensionList = QList<FileExtension>;

class FileExtensionUtils
{
public:
    static bool isExecutable(FileExtension extension);
};

class FileExtensionModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

public:
    enum Role
    {
        ExtensionRole = Qt::UserRole + 1
    };

    explicit FileExtensionModel(QObject* parent = nullptr);
    virtual ~FileExtensionModel() override;

    FileExtensionList extensions() const;
    void setExtensions(const FileExtensionList& extensions);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

private:
    FileExtensionList m_data;
};

struct Filename
{
    QString path;
    QString name; //< Full file name without extension.
    FileExtension extension = FileExtension::mkv;

    static Filename parse(const QString& filename);

    QString completeFileName() const;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::FileExtension)
