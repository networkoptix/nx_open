
#ifndef EC2_STORED_FILE_MANAGER_H
#define EC2_STORED_FILE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnStoredFileManager
    :
        public AbstractStoredFileManager
    {
    public:
        QnStoredFileManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) override;
        virtual int addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) override;
        virtual int deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) override;
        virtual int listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnCameraManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiStoredFileData>( const QnTransaction<ApiStoredFileData>& tran )
        {
            if( tran.command == ApiCommand::addStoredFile )
                emit added( tran.params.path );
            else if( tran.command == ApiCommand::updateStoredFile )
                emit updated( tran.params.path );
            else
                assert( false );
        }

        template<> void triggerNotification<QString>( const QnTransaction<QString>& tran )
        {
            assert( tran.command == ApiCommand::removeStoredFile );
            emit removed( tran.params );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        const ResourceContext m_resCtx;
    };
}

#endif  //EC2_STORED_FILE_MANAGER_H
