/**********************************************************
* 4 feb 2014
* akolesnikov
***********************************************************/

#ifndef FIXED_URL_CLIENT_QUERY_PROCESSOR_H
#define FIXED_URL_CLIENT_QUERY_PROCESSOR_H

#include <memory>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include "client_query_processor.h"


namespace ec2
{
    //!Calls \a ClientQueryProcessor::processQueryAsync with pre-specified EC url
    /*!
        \warning \a ClientQueryProcessor instance MUST be alive for the whole \a FixedUrlClientQueryProcessor live time
    */
    class FixedUrlClientQueryProcessor
    {
    public:
        FixedUrlClientQueryProcessor( ClientQueryProcessor* clientProcessor, const QUrl& ecURL )
        :
            m_clientProcessor( clientProcessor ),
            m_ecURL( ecURL )
        {
        }

        template<class QueryDataType, class HandlerType>
            void processUpdateAsync( const QnTransaction<QueryDataType>& tran, HandlerType handler )
        {
            QUrl ecUrl;
            {
                QMutexLocker lk( &m_mutex );
                ecUrl = m_ecURL;
            }
            m_clientProcessor->processUpdateAsync( ecUrl, tran, handler );
        }

        template<class T> bool processIncomingTransaction( const QnTransaction<T>&  tran, const QByteArray& serializedTran)
        {
            return m_clientProcessor->processIncomingTransaction( tran, serializedTran );
        }

        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
            QUrl ecUrl;
            {
                QMutexLocker lk( &m_mutex );
                ecUrl = m_ecURL;
            }
            m_clientProcessor->processQueryAsync<InputData, OutputData, HandlerType>( ecUrl, cmdCode, input, handler );
        }

        QUrl getUrl() const
        {
            QMutexLocker lk( &m_mutex );
            return m_ecURL;
        }

        QString userName() const
        {
            QMutexLocker lk( &m_mutex );
            return m_ecURL.userName();
        }

        void setPassword( const QString& password )
        {
            QMutexLocker lk( &m_mutex );
            m_ecURL.setPassword( password );
        }

    private:
        ClientQueryProcessor* m_clientProcessor;
        QUrl m_ecURL;
        mutable QMutex m_mutex;
    };

    typedef std::shared_ptr<FixedUrlClientQueryProcessor> FixedUrlClientQueryProcessorPtr;
}

#endif  //FIXED_URL_CLIENT_QUERY_PROCESSOR_H
