#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>

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

class FileSystemStrings
{
    Q_DECLARE_TR_FUNCTIONS(FileSystemStrings)

public:
    static QString suffix(FileExtension ext);
    static FileExtension extension(const QString& suffix,
        FileExtension defaultValue = FileExtension::mkv);
    static QString description(FileExtension extension);
    static QString filterDescription(FileExtension ext);
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

    bool operator==(const Filename& other) const;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::FileExtension)
