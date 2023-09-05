// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_export_processor.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QtCore/QString>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/utils/table_export_helper.h>

namespace nx::vms::client::desktop {

class LookupListExportProcessor::Private: public QObject
{
    LookupListExportProcessor* const q = nullptr;

public:
    Private(LookupListExportProcessor* q);
    void processExportTaskResult();
    static ExportExitCode runTask(
        QVector<int> rows, LookupListEntriesModel* model, const QString& filename);
    QFutureWatcher<ExportExitCode> currentTask;
    QString currentExportFolder;
};

LookupListExportProcessor::Private::Private(LookupListExportProcessor* q):
    QObject(),
    q(q)
{
}

LookupListExportProcessor::ExportExitCode LookupListExportProcessor::Private::runTask(
    QVector<int> rows, LookupListEntriesModel* model, const QString& filename)
{
    if (!NX_ASSERT(model))
        return InternalError;

    QFile file(filename);
    if (file.open(QFile::WriteOnly))
    {
        QTextStream streamCsv(&file);
        model->exportEntries(QSet<int>(rows.begin(), rows.end()), streamCsv);
        if (streamCsv.status() == QTextStream::Ok)
            return Success;
    }
    return ErrorFileNotSaved;
}

void LookupListExportProcessor::Private::processExportTaskResult()
{
    emit q->exportFinished(currentTask.isCanceled() ? Canceled : currentTask.result());
}

LookupListExportProcessor::LookupListExportProcessor(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LookupListExportProcessor::~LookupListExportProcessor()
{
}

void LookupListExportProcessor::cancelExport()
{
    if (d->currentTask.isRunning())
    {
        d->currentTask.cancel();
    }
}

QUrl LookupListExportProcessor::exportFolder() const
{
    return QUrl::fromLocalFile(d->currentExportFolder);
}

bool LookupListExportProcessor::exportListEntries(
    const QVector<int>& rows, LookupListEntriesModel* model)
{
    if (d->currentTask.isRunning())
    {
        // For now support only one export process.
        return false;
    }

    const QString fileFilters =
        QnCustomFileDialog::createFilter({{tr("Text files"), {"csv"}}, {tr("All"), {"*"}}});
    const auto fileName =
        QnTableExportHelper::getExportFilePathFromDialog(fileFilters, nullptr, tr("Export List"));
    if (fileName.isEmpty())
        return false;
    d->currentExportFolder = QFileInfo(fileName).absolutePath();

    auto future =
        QtConcurrent::run(LookupListExportProcessor::Private::runTask, rows, model, fileName);
    d->currentTask.setFuture(future);

    connect(&d->currentTask,
        &QFutureWatcherBase::started,
        this,
        &LookupListExportProcessor::exportStarted);
    connect(&d->currentTask,
        &QFutureWatcherBase::finished,
        d.get(),
        &LookupListExportProcessor::Private::processExportTaskResult);

    return true;
}

} // namespace nx::vms::client::desktop
