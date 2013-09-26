/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef GETFILEOPERATION_H
#define GETFILEOPERATION_H

#include <stdint.h>

#include <atomic>
#include <memory>
#include <mutex>

#include <QObject>
#include <QString>
#include <QUrl>

#include <utils/common/systemerror.h>
#include <utils/fs/file.h>

#include "rdir_synchronization_operation.h"


namespace nx_http
{
    class AsyncHttpClient;
}

namespace detail
{
    /*!
        Object of this class MUST be used only as std::shared_ptr<>
    */
    class GetFileOperation
    :
        public QObject,
        public RDirSynchronizationOperation,
        public std::enable_shared_from_this<GetFileOperation>,
        public QnFile::AbstractWriteHandler,
        public QnFile::AbstractCloseHandler
    {
        Q_OBJECT

    public:
        GetFileOperation(
            int _id,
            const QUrl& baseUrl,
            const QString& filePath,
            const QString& localDirPath,
            const QString& hashTypeName,
            AbstractRDirSynchronizationEventHandler* _handler );
        virtual ~GetFileOperation();

        //!Implementation of QnStoppable::pleaseStop
        virtual void pleaseStop() override;

        //!Implementation of RDirSynchronizationOperation::startAsync
        virtual bool startAsync() override;

    private:
        enum class State
        {
            sInit,
            sCheckingHashPresence,
            sCheckingHash,
            sDownloadingFile,
            //!Received all data from remote side, waiting for data to be written
            sWaitingForWriteToFinish,
            sFinished
        };

        const QString m_filePath;
        nx_http::AsyncHttpClient* m_httpClient;
        State m_state;
        const QString m_localDirPath;
        const QString m_hashTypeName;
        std::shared_ptr<QnFile> m_outFile;
        std::atomic_int m_fileWritePending;
        mutable std::mutex m_mutex;

        //!Implementation of QnFile::AbstractWriteHandler::onAsyncWriteFinished
        virtual void onAsyncWriteFinished(
            const std::shared_ptr<QnFile>& file,
            uint64_t bytesWritten,
            SystemError::ErrorCode errorCode ) override;
        //!Implementation of QnFile::AbstractCloseHandler::onAsyncCloseFinished
        virtual void onAsyncCloseFinished(
            const std::shared_ptr<QnFile>& file,
            SystemError::ErrorCode errorCode ) override;
        bool onEnteredDownloadingFile();
        void onSomeMessageBodyAvailableNonSafe( nx_http::AsyncHttpClient* httpClient );

    private slots:
        void onResponseReceived( nx_http::AsyncHttpClient* );
        void onSomeMessageBodyAvailable( nx_http::AsyncHttpClient* );
        void onHttpDone( nx_http::AsyncHttpClient* );
    };
}

#endif  //GETFILEOPERATION_H
