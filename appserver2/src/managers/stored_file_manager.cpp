
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
    int QnStoredFileManager<QueryProcessorType>::getStoredFile( const QString& /*filename*/, impl::GetStoredFileHandlerPtr /*handler*/ )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::addStoredFile( const QString& /*filename*/, const QByteArray& /*data*/, impl::SimpleHandlerPtr /*handler*/ )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::deleteStoredFile( const QString& /*filename*/, impl::SimpleHandlerPtr /*handler*/ )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }

    template<class QueryProcessorType>
    int QnStoredFileManager<QueryProcessorType>::listDirectory( const QString& /*folderName*/, impl::ListDirectoryHandlerPtr /*handler*/ )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }


    template class QnStoredFileManager<ServerQueryProcessor>;
    template class QnStoredFileManager<FixedUrlClientQueryProcessor>;
}
