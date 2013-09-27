/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef RDIR_SYNCHRONIZATION_OPERATION_H
#define RDIR_SYNCHRONIZATION_OPERATION_H

#include <memory>

#include <utils/common/stoppable.h>


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

        //!Called in some unknown aio thread
        /*!
            \note MUST NOT block!
        */
        virtual void operationDone( const std::shared_ptr<RDirSynchronizationOperation>& operation ) = 0;
    };

    class RDirSynchronizationOperation
    :
        public QnStoppable
    {
    public:
        enum class ResultCode
        {
            success,
            downloadFailure,
            writeFailure,
            interrupted,
            unknownError
        };

        const RSyncOperationType type;
        const int id;
        const QUrl baseUrl;
        const QString entryPath;

        RDirSynchronizationOperation(
            RSyncOperationType _type,
            int _id,
            const QUrl& _baseUrl,
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

    protected:
        AbstractRDirSynchronizationEventHandler* const m_handler;

        void setErrorText( const QString& _errorText ) { m_errorText = _errorText; }
        void setResult( ResultCode _result ) { m_result = _result; }

    private:
        QString m_errorText;
        ResultCode m_result;
    };
}

#endif  //RDIR_SYNCHRONIZATION_OPERATION_H
