#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QAbstractListModel>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(desktop, FileExtension, )

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

    bool operator==(const Filename& other) const;
};

} // namespace desktop
} // namespace client
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::FileExtension, (metatype)(lexical))
