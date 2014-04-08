
#include "stored_file_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnStoredFileManager<QueryProcessorType>::QnStoredFileManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx )
    {
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiStoredFileData& fileData) {
            handler->done( reqID, errorCode, fileData.data );
        };
        m_queryProcessor->template processQueryAsync<QString, ApiStoredFileData, decltype(queryDoneHandler)> ( ApiCommand::getStoredFile, filename, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( filename, data );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( filename );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiStoredDirContents& folderContents) {
            QStringList outputFolderContents;
            std::copy( folderContents.begin(), folderContents.end(), std::back_inserter(outputFolderContents) );
            handler->done( reqID, errorCode, outputFolderContents );
        };
        m_queryProcessor->template processQueryAsync<ApiStoredFilePath, ApiStoredDirContents, decltype(queryDoneHandler)>( ApiCommand::listDirectory, (ApiStoredFilePath) folderName, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiStoredFileData> QnStoredFileManager<QueryProcessorType>::prepareTransaction( const QString& filename, const QByteArray& data )
    {
        QnTransaction<ApiStoredFileData> tran(ApiCommand::addStoredFile, true);
        tran.params.path = filename;
        tran.params.data = data;
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiStoredFilePath> QnStoredFileManager<QueryProcessorType>::prepareTransaction( const QString& filename )
    {
        QnTransaction<ApiStoredFilePath> tran(ApiCommand::removeStoredFile, true);
        tran.params = filename;
        return tran;
    }


    template class QnStoredFileManager<ServerQueryProcessor>;
    template class QnStoredFileManager<FixedUrlClientQueryProcessor>;
}
