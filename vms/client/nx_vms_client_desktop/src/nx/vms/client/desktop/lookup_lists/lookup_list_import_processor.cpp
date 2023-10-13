// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_import_processor.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QtCore/QMap>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>

#include <nx/vms/client/desktop/lookup_lists/lookup_list_entries_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_model.h>
#include <ui/dialogs/common/custom_file_dialog.h>

namespace nx::vms::client::desktop {

class LookupListImportProcessor::Private: public QObject
{
    LookupListImportProcessor* const q = nullptr;

public:
    Private(LookupListImportProcessor* q);
    void processImportTaskResult();
    static ImportExitCode runTask(const QString sourceFile,
        const QString separator,
        bool importHeaders,
        LookupListEntriesModel* model);
    QFutureWatcher<ImportExitCode> currentTask;
    QMap<QPair<int, QString>, QString> clarifications;
};

LookupListImportProcessor::Private::Private(LookupListImportProcessor* q): QObject(), q(q)
{
}

/** Fill temporary model and create list of replacements. Then after call for next dialog apply
 * replacements and merge to main model.
 */
LookupListImportProcessor::ImportExitCode LookupListImportProcessor::Private::runTask(
    const QString sourceFile,
    const QString separator,
    bool importHeaders,
    LookupListEntriesModel* model)
{
    if (!NX_ASSERT(model))
        return InternalError;

    QFile file(sourceFile);
    if (!file.open(QFile::ReadOnly))
        return ErrorFileNotFound;

    QTextStream streamCsv(&file);
    if (importHeaders)
    {
        // Currently just skip headers if them are in file
        streamCsv.readLine();
        //TODO @pprivalov implement this later
        //model->updateHeaders(streamCsv.readLine().split(separator));
    }

    QScopedPointer<LookupListEntriesModel> tempModel(new LookupListEntriesModel());
    auto listModel = QScopedPointer<LookupListModel>(new LookupListModel());
    tempModel->setListModel(listModel.get());
    tempModel->listModel()->setAttributeNames(model->listModel()->attributeNames());

    QVariantMap entry;
    for (auto line = streamCsv.readLine(); !line.isEmpty(); line = streamCsv.readLine())
    {
        auto words = line.split(separator);
        for (int i = 0; i < model->columnCount(); ++i)
        {
            if (i >= words.size())
                break;

            entry[model->listModel()->attributeNames()[i]] = words[i];
        }
        tempModel->addEntry(entry);
    }

    if (tempModel->rowCount() == 0)
        return EmptyFileError;
    /*
    // TODO: @pprivalov Handle replacements
    if (m_tempModel->validate())
        return ReplacementRequired;
    */

    // TODO: move to separate method after replacements are applied
    for (const auto& entry: tempModel->listModel()->rawData().entries)
    {
        QVariantMap varianEntry;
        for (auto pair: entry)
            varianEntry[pair.first] = pair.second;
        model->addEntry(varianEntry);
    }

    return Success;
}

void LookupListImportProcessor::Private::processImportTaskResult()
{
    emit q->importFinished(currentTask.isCanceled() ? Canceled : currentTask.result());
}

// ------------------------------------------------------------------------------------------------------

LookupListImportProcessor::LookupListImportProcessor(QObject* parent):
    base_type(parent), d(new Private(this))
{
}

LookupListImportProcessor::~LookupListImportProcessor()
{
}

void LookupListImportProcessor::cancelImport()
{
    if (d->currentTask.isRunning())
        d->currentTask.cancel();
}

bool LookupListImportProcessor::importListEntries(const QString sourceFile,
    const QString separator,
    const bool importHeaders,
    LookupListEntriesModel* model)
{
    if (d->currentTask.isRunning())
        return false;

    if (sourceFile.isEmpty() || separator.isEmpty())
        return false;

    auto future = QtConcurrent::run(
        LookupListImportProcessor::Private::runTask, sourceFile, separator, importHeaders, model);
    d->currentTask.setFuture(future);

    connect(&d->currentTask,
        &QFutureWatcherBase::started,
        this,
        &LookupListImportProcessor::importStarted);
    connect(&d->currentTask,
        &QFutureWatcherBase::finished,
        d.get(),
        &LookupListImportProcessor::Private::processImportTaskResult);

    return true;
}

} // namespace nx::vms::client::desktop
