
#ifndef EC2_STORED_FILE_MANAGER_H
#define EC2_STORED_FILE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    class QnStoredFileNotificationManager
    :
        public AbstractStoredFileManager
    {
    public:
        QnStoredFileNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran )
        {
            if( tran.command == ApiCommand::addStoredFile )
                emit added( tran.params.path );
            else if( tran.command == ApiCommand::updateStoredFile )
                emit updated( tran.params.path );
            else
                assert( false );
        }

        void triggerNotification( const QnTransaction<ApiStoredFilePath>& tran )
        {
            assert( tran.command == ApiCommand::removeStoredFile );
            emit removed( tran.params.path );
        }

    private:
        const ResourceContext m_resCtx;
    };


    template<class QueryProcessorType>
    class QnStoredFileManager
    :
        public QnStoredFileNotificationManager
    {
    public:
        QnStoredFileManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) override;
        virtual int addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) override;
        virtual int deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) override;
        virtual int listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiStoredFileData> prepareTransaction( const QString& filename, const QByteArray& data );
        QnTransaction<ApiStoredFilePath> prepareTransaction( const QString& filename );
    };
}

#endif  //EC2_STORED_FILE_MANAGER_H
