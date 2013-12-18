/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef GETFILEOPERATION_H
#define GETFILEOPERATION_H

#include <stdint.h>

#include <memory>
#include <mutex>

#include <QObject>
#include <QString>
#include <QUrl>

#include <utils/common/systemerror.h>
#include <utils/fs/file.h>
#include <utils/network/http/asynchttpclient.h>

#include "rdir_synchronization_operation.h"


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
            //!Received all data from remote side, waiting for data to be written to the file
            sWaitingForWriteToFinish,
            //!file download has been interrupted (because of error or \a GetFileOperation::pleaseStop() call)
            sInterrupted,
            sFinished
        };

        const QString m_filePath;
        nx_http::AsyncHttpClientPtr m_httpClient;
        State m_state;
        const QString m_localDirPath;
        const QString m_hashTypeName;
        std::shared_ptr<QnFile> m_outFile;
        int m_fileWritePending;
        mutable std::mutex m_mutex;
        uint64_t m_totalBytesDownloaded;
        uint64_t m_totalBytesWritten;

        //!Implementation of QnFile::AbstractWriteHandler::onAsyncWriteFinished
        virtual void onAsyncWriteFinished(
            const std::shared_ptr<QnFile>& file,
            uint64_t bytesWritten,
            SystemError::ErrorCode errorCode ) override;
        //!Implementation of QnFile::AbstractCloseHandler::onAsyncCloseFinished
        virtual void onAsyncCloseFinished(
            const std::shared_ptr<QnFile>& file,
            SystemError::ErrorCode errorCode ) override;
        bool startFileDownload();
        void onSomeMessageBodyAvailableNonSafe();

    private slots:
        void onResponseReceived( nx_http::AsyncHttpClientPtr );
        void onSomeMessageBodyAvailable( nx_http::AsyncHttpClientPtr );
        void onHttpDone( nx_http::AsyncHttpClientPtr );
    };
}

#endif  //GETFILEOPERATION_H
