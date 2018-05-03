/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef RDIR_SYNCHRONIZATION_OPERATION_H
#define RDIR_SYNCHRONIZATION_OPERATION_H

#include <memory>

#include <QtCore/QUrl>

#include <nx/utils/thread/stoppable.h>
#include <nx/utils/url.h>


namespace detail
{
    class RDirSynchronizationOperation;

    enum class RSyncOperationType
    {
        none,
        listDirectory,
        getFile
    };

    class AbstractRDirSynchronizationEventHandler
    {
    public:
        virtual ~AbstractRDirSynchronizationEventHandler() {}

        /*!
            \param remoteFileSize -1 if file size if unknown (not reported by http server)
        */
        virtual void downloadProgress(
            const std::shared_ptr<RDirSynchronizationOperation>& /*operation*/,
            int64_t /*remoteFileSize*/,
            int64_t /*bytesDownloaded*/ ) {};
        //!Called in some unknown aio thread
        /*!
            \note MUST NOT block!
        */
        virtual void operationDone( const std::shared_ptr<RDirSynchronizationOperation>& operation ) = 0;
    };

    namespace ResultCode
    {
        enum Value
        {
            success,
            downloadFailure,
            writeFailure,
            interrupted,
            unknownError
        };

        QString toString( Value val );
    }

    class RDirSynchronizationOperation
    :
        public QnStoppable
    {
    public:
        const RSyncOperationType type;
        const int id;
        const nx::utils::Url baseUrl;
        const QString entryPath;

        RDirSynchronizationOperation(
            RSyncOperationType _type,
            int _id,
            const nx::utils::Url& _baseUrl,
            const QString& _entryPath,
            AbstractRDirSynchronizationEventHandler* handler )
        :
            type( _type ),
            id( _id ),
            baseUrl( _baseUrl ),
            entryPath( _entryPath ),
            m_handler( handler ),
            m_result( ResultCode::success )
        {
        }

        virtual bool startAsync() = 0;
        bool success() const { return m_result == ResultCode::success; }
        //!Returns true, if failed to get data from remote side
        bool remoteSideFailure() const { return m_result == ResultCode::downloadFailure; }
        QString errorText() const { return m_errorText; }
        ResultCode::Value result() const { return m_result; };

    protected:
        AbstractRDirSynchronizationEventHandler* const m_handler;

        void setErrorText( const QString& _errorText ) { m_errorText = _errorText; }
        void setResult( ResultCode::Value _result ) { m_result = _result; }

    private:
        QString m_errorText;
        ResultCode::Value m_result;
    };
}

#endif  //RDIR_SYNCHRONIZATION_OPERATION_H
