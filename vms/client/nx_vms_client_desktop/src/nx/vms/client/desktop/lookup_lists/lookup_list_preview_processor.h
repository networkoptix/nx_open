// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QAbstractTableModel>

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_import_entries_model.h"

namespace nx::vms::client::desktop {

/**
 * Allows to call static functions in QML without creation of the LookupListPreviewProcessor.
 */
class NX_VMS_CLIENT_DESKTOP_API LookupListPreviewHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LookupListPreviewHelper(QObject* parent = nullptr);
    static Q_INVOKABLE QString getImportFilePathFromDialog();
    static LookupListPreviewHelper* instance();
};

/**
 * Processor of reading input csv/tsv file for generating preview data in
 * LookupListImportEntriesModel. Used in QML.
 */
class NX_VMS_CLIENT_DESKTOP_API LookupListPreviewProcessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum PreviewBuildResult
    {
        Success,
        InternalError,
        EmptyFileError,
        ErrorFileNotFound,
    }

    Q_ENUMS(PreviewBuildResult)

    Q_PROPERTY(int rowsNumber READ rowsNumber WRITE setRowsNumber
        NOTIFY rowsNumberChanged REQUIRED);
    Q_PROPERTY(QString separator READ separator WRITE setSeparator NOTIFY separatorChanged);
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged);
    Q_PROPERTY(bool dataHasHeaderRow READ dataHasHeaderRow WRITE setDataHasHeaderRow
        NOTIFY dataHasHeaderRowChanged);
    Q_PROPERTY(bool valid READ valid WRITE setValid NOTIFY validChanged)

    explicit LookupListPreviewProcessor(QObject* parent = nullptr);
    virtual ~LookupListPreviewProcessor() override;
    Q_INVOKABLE bool setImportFilePathFromDialog();
    Q_INVOKABLE PreviewBuildResult buildTablePreview(LookupListImportEntriesModel* model,
        const QString& filePath,
        const QString& separator,
        bool hasHeader);

    void setRowsNumber(int rowsNumber);
    void setSeparator(const QString& separator);
    void setFilePath(const QString& filePath);
    void setDataHasHeaderRow(bool dataHasHeaderRow);
    void setValid(bool isValid);

    int rowsNumber();
    QString separator();
    QString filePath();
    bool dataHasHeaderRow();
    bool valid();
signals:
    void rowsNumberChanged(int rowsNumber);
    void separatorChanged(const QString& separator);
    void filePathChanged(const QString& filePath);
    void dataHasHeaderRowChanged(bool dataHasHeaderRow);
    void validChanged(bool isValid);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
