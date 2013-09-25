/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef GETFILEOPERATION_H
#define GETFILEOPERATION_H

#include <memory>

#include <QObject>
#include <QString>
#include <QUrl>

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
        public std::enable_shared_from_this<GetFileOperation>
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
            sDownloadingFile
        };

        const QString m_filePath;
        nx_http::AsyncHttpClient* m_httpClient;
        State m_state;
        const QString m_localDirPath;
        const QString m_hashTypeName;

        bool onEnteredDownloadingFile();

    private slots:
        void onResponseReceived( nx_http::AsyncHttpClient* );
        void onSomeMessageBodyAvailable( nx_http::AsyncHttpClient* );
        void onHttpDone( nx_http::AsyncHttpClient* );
    };
}

#endif  //GETFILEOPERATION_H
