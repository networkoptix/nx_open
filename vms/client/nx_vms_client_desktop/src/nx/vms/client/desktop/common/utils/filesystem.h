// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>

namespace nx::vms::client::desktop {

/** List of supported file extensions, used in the client. */
enum class FileExtension
{
    avi,
    mkv,
    mp4,
    nov,
    exe,
};
using FileExtensionList = QList<FileExtension>;

class FileExtensionUtils
{
public:
    static bool isLayout(FileExtension extension);
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

struct NX_VMS_CLIENT_DESKTOP_API Filename
{
    QString path;
    QString name; //< Full file name without extension.
    FileExtension extension = FileExtension::mkv;

    static Filename parse(const QString& filename);

    QString completeFileName() const;

    bool operator==(const Filename& other) const;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::FileExtension)
