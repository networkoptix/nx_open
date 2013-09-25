/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef LIST_DIRECTORY_OPERATION_H
#define LIST_DIRECTORY_OPERATION_H

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
    class RDirEntry
    {
    public:
        RDirEntry(
            QString _entryPath = QString(),
            detail::RSyncOperationType _type = detail::RSyncOperationType::none,
            QString _hashTypeName = QString() )
        :
            entryPath( _entryPath ),
            type( _type ),
            hashTypeName( _hashTypeName )
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
        ListDirectoryOperation(
            int _id,
            const QUrl& baseUrl,
            const QString& _dirPath,
            AbstractRDirSynchronizationEventHandler* _handler );
        ~ListDirectoryOperation();

        //!Implementation of QnStoppable::pleaseStop
        virtual void pleaseStop() override;

        //!Implementation of RDirSynchronizationOperation::startAsync
        virtual bool startAsync() override;

        std::list<detail::RDirEntry> entries() const;

    private:
        const QString m_dirPath;
        nx_http::AsyncHttpClient* m_httpClient;
        std::list<detail::RDirEntry> m_entries;
        QUrl m_downloadUrl;

    private slots:
        void onResponseReceived( nx_http::AsyncHttpClient* );
        void onHttpDone( nx_http::AsyncHttpClient* );
    };
}

#endif  //LIST_DIRECTORY_OPERATION_H
