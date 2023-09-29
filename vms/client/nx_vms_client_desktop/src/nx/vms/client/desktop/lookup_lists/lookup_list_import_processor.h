// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

typedef QMap<QPair<int, QString>, QString> Clarifications;

class LookupListImportProcessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit LookupListImportProcessor(QObject* parent = nullptr);
    virtual ~LookupListImportProcessor() override;
    Q_INVOKABLE bool importListEntries(const QString sourceFile,
        const QString separator,
        const bool importHeaders,
        LookupListEntriesModel* model);
    Q_INVOKABLE void cancelImport();

    enum ImportExitCode
    {
        Success,
        Canceled,
        InternalError,
        ErrorFileNotFound
    };

    Q_ENUMS(ImportExitCode)

signals:
    void importStarted();
    void importFinished(const ImportExitCode code);
    void importClarificationsRequired(Clarifications& clarifications);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
