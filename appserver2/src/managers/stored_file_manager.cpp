
#include "stored_file_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnStoredFileManager<QueryProcessorType>::QnStoredFileManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData)
    :
      m_queryProcessor( queryProcessor ),
      m_userAccessData(userAccessData)
    {
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiStoredFileData& fileData) {
            handler->done( reqID, errorCode, fileData.data );
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<ApiStoredFilePath, ApiStoredFileData, decltype(queryDoneHandler)> ( ApiCommand::getStoredFile, filename, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiStoredFileData params;
        params.path = filename;
        params.data = data;

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::addStoredFile, params,
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeStoredFile, ApiStoredFilePath(filename),
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiStoredFilePathList& folderContents) {
            QStringList outputFolderContents;
            std::transform(folderContents.cbegin(), folderContents.cend(), std::back_inserter(outputFolderContents), [](const ApiStoredFilePath &path) {return path.path; } );
            handler->done( reqID, errorCode, outputFolderContents );
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<ApiStoredFilePath, ApiStoredFilePathList, decltype(queryDoneHandler)>( ApiCommand::listDirectory, ApiStoredFilePath(folderName), queryDoneHandler );
        return reqID;
    }

    template class QnStoredFileManager<ServerQueryProcessorAccess>;
    template class QnStoredFileManager<FixedUrlClientQueryProcessor>;
}
