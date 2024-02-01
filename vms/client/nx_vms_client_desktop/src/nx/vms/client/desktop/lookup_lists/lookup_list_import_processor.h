// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_import_entries_model.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API LookupListImportProcessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit LookupListImportProcessor(QObject* parent = nullptr);
    virtual ~LookupListImportProcessor() override;
    Q_INVOKABLE bool importListEntries(const QString sourceFile,
        const QString separator,
        const bool importHeaders,
        LookupListImportEntriesModel* model);
    Q_INVOKABLE bool applyFixUps(LookupListImportEntriesModel* model);
    Q_INVOKABLE void cancelRunningTask();

    enum ImportExitCode
    {
        Success,
        ClarificationRequired,
        Canceled,
        InternalError,
        EmptyFileError,
        ErrorFileNotFound
    };

    Q_ENUMS(ImportExitCode)

signals:
    void importStarted();
    void importFinished(ImportExitCode code);
    void fixupStarted();
    void fixupFinished(ImportExitCode code);
private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
