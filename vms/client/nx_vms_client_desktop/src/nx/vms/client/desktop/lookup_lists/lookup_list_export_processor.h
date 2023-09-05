// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVector>

#include <nx/utils/impl_ptr.h>

#include "lookup_list_entries_model.h"

namespace nx::vms::client::desktop {

class LookupListExportProcessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit LookupListExportProcessor(QObject* parent = nullptr);
    virtual ~LookupListExportProcessor() override;
    Q_INVOKABLE bool exportListEntries(const QVector<int>& rows, LookupListEntriesModel* model);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE QUrl exportFolder() const;

    enum ExportExitCode
    {
        Success,
        Canceled,
        InternalError,
        ErrorFileNotSaved
    };

    Q_ENUMS(ExportExitCode)

signals:
    void exportStarted();
    void exportFinished(ExportExitCode code);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
