
#ifndef EC2_STORED_FILE_MANAGER_H
#define EC2_STORED_FILE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    class QnStoredFileNotificationManager : public AbstractStoredFileNotificationManager
    {
    public:
        QnStoredFileNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran )
        {
            if( tran.command == ApiCommand::addStoredFile )
                emit added( tran.params.path );
            else if( tran.command == ApiCommand::updateStoredFile )
                emit updated( tran.params.path );
            else
                NX_ASSERT( false );
        }

        void triggerNotification( const QnTransaction<ApiStoredFilePath>& tran )
        {
            NX_ASSERT( tran.command == ApiCommand::removeStoredFile );
            emit removed( tran.params.path );
        }
    };

    typedef std::shared_ptr<QnStoredFileNotificationManager> QnStoredFileNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnStoredFileManager : public AbstractStoredFileManager
    {
    public:
        QnStoredFileManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        virtual int getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) override;
        virtual int addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) override;
        virtual int deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) override;
        virtual int listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiStoredFileData> prepareTransaction( const QString& filename, const QByteArray& data );
        QnTransaction<ApiStoredFilePath> prepareTransaction( const QString& filename );
    };
}

#endif  //EC2_STORED_FILE_MANAGER_H
