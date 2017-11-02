/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef LIST_DIRECTORY_OPERATION_H
#define LIST_DIRECTORY_OPERATION_H

#include <memory>

#include <QObject>
#include <QString>
#include <nx/network/http/asynchttpclient.h>

#include "rdir_synchronization_operation.h"


namespace detail
{
    class RDirEntry
    {
    public:
        RDirEntry(
            QString _entryPath = QString(),
            detail::RSyncOperationType _type = detail::RSyncOperationType::none,
            QString _hashTypeName = QString(),
            qint64 _entrySize = -1 )
        :
            entryPath( _entryPath ),
            type( _type ),
            hashTypeName( _hashTypeName ),
            entrySize( _entrySize )
        {
        }

        //!path is relative to \a localDirPath
        /*!
            if \a type == \a tListDirectory, then directory path is stored here (not a path to 'contents.xml' file)
        */
        QString entryPath;
        detail::RSyncOperationType type;
        //!if not empty, file *.{hashTypeName} is tried first
        /*!
            Currently, only md5 is supported
        */
        QString hashTypeName;
        qint64 entrySize;
    };

    /*!
        Object of this class MUST be used only as std::shared_ptr<>
    */
    class ListDirectoryOperation
    :
        public QObject,
        public RDirSynchronizationOperation,
        public std::enable_shared_from_this<ListDirectoryOperation>
    {
        Q_OBJECT

    public:
        /*!
            \param localTargetDirPath Local directory to sync to
        */
        ListDirectoryOperation(int _id,
            const nx::utils::Url &baseUrl,
            const QString& _dirPath,
            const QString& localTargetDirPath,
            AbstractRDirSynchronizationEventHandler* _handler );
        ~ListDirectoryOperation();

        //!Implementation of QnStoppable::pleaseStop
        virtual void pleaseStop() override;

        //!Implementation of RDirSynchronizationOperation::startAsync
        virtual bool startAsync() override;

        std::list<detail::RDirEntry> entries() const;
        int64_t totalDirSize() const;

    private:
        const QString m_localTargetDirPath;
        nx_http::AsyncHttpClientPtr m_httpClient;
        int64_t m_totalsize;
        std::list<detail::RDirEntry> m_entries;
        nx::utils::Url m_downloadUrl;

    private slots:
        void onResponseReceived( nx_http::AsyncHttpClientPtr );
        void onHttpDone( nx_http::AsyncHttpClientPtr );
    };
}

#endif  //LIST_DIRECTORY_OPERATION_H
