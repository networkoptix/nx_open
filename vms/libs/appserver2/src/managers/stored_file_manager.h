// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/stored_file_data.h>
#include <nx_ec/managers/abstract_stored_file_manager.h>

#include "transaction/transaction.h"

namespace ec2 {

template<class QueryProcessorType>
class QnStoredFileManager : public AbstractStoredFileManager
{
public:
    QnStoredFileManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getStoredFile(
        const QString& fileName,
        Handler<QByteArray> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addStoredFile(
        const QString& fileName,
        const QByteArray& fileData,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int deleteStoredFile(
        const QString& fileName,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int listDirectory(
        const QString& directoryName,
        Handler<QStringList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnStoredFileManager<QueryProcessorType>::QnStoredFileManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnStoredFileManager<QueryProcessorType>::getStoredFile(
    const QString& fileName,
    Handler<QByteArray> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    using namespace nx::vms::api;
    const int requestId = generateRequestID();
    processor().template processQueryAsync<StoredFilePath, StoredFileData>(
        ApiCommand::getStoredFile,
        fileName,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](
            Result result, const nx::vms::api::StoredFileData& fileData) mutable
        {
            handler(requestId, std::move(result), fileData.data);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnStoredFileManager<QueryProcessorType>::addStoredFile(
    const QString& fileName,
    const QByteArray& fileData,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::StoredFileData params;
    params.path = fileName;
    params.data = fileData;

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::addStoredFile,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnStoredFileManager<QueryProcessorType>::deleteStoredFile(
    const QString& fileName,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeStoredFile,
        nx::vms::api::StoredFilePath(fileName),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnStoredFileManager<QueryProcessorType>::listDirectory(
    const QString& directoryName,
    Handler<QStringList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    using namespace nx::vms::api;
    const int requestId = generateRequestID();
    processor().template processQueryAsync<StoredFilePath, StoredFilePathList>(
        ApiCommand::listDirectory,
        nx::vms::api::StoredFilePath(directoryName),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](
            Result result, const nx::vms::api::StoredFilePathList& folderContents) mutable
        {
            QStringList outputFolderContents;
            std::transform(
                folderContents.cbegin(),
                folderContents.cend(),
                std::back_inserter(outputFolderContents),
                [](const nx::vms::api::StoredFilePath& path) { return path.path; });
            handler(requestId, std::move(result), std::move(outputFolderContents));
        });
    return requestId;
}

} // namespace ec2
