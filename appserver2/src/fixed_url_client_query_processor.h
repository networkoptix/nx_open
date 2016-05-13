/**********************************************************
* 4 feb 2014
* akolesnikov
***********************************************************/

#ifndef FIXED_URL_CLIENT_QUERY_PROCESSOR_H
#define FIXED_URL_CLIENT_QUERY_PROCESSOR_H

#include <memory>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>
#include <core/resource_management/user_access_data.h>

#include "client_query_processor.h"


namespace ec2
{
    //!Calls \a ClientQueryProcessor::processQueryAsync with pre-specified Server url
    /*!
        \warning \a ClientQueryProcessor instance MUST be alive for the whole \a FixedUrlClientQueryProcessor live time
    */
    class FixedUrlClientQueryProcessor
    {
    public:
        FixedUrlClientQueryProcessor &getAccess(const Qn::UserAccessData &) { return *this; }

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
                QnMutexLocker lk( &m_mutex );
                ecUrl = m_ecURL;
            }
            m_clientProcessor->processUpdateAsync( ecUrl, tran, handler );
        }

        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
            QUrl ecUrl;
            {
                QnMutexLocker lk( &m_mutex );
                ecUrl = m_ecURL;
            }
            m_clientProcessor->processQueryAsync<InputData, OutputData, HandlerType>( ecUrl, cmdCode, input, handler );
        }

        QUrl getUrl() const
        {
            QnMutexLocker lk( &m_mutex );
            return m_ecURL;
        }

        QString userName() const
        {
            QnMutexLocker lk( &m_mutex );
            return m_ecURL.userName();
        }

        void setPassword( const QString& password )
        {
            QnMutexLocker lk( &m_mutex );
            m_ecURL.setPassword( password );
        }

    private:
        ClientQueryProcessor* m_clientProcessor;
        QUrl m_ecURL;
        mutable QnMutex m_mutex;
    };

    typedef std::shared_ptr<FixedUrlClientQueryProcessor> FixedUrlClientQueryProcessorPtr;
}

#endif  //FIXED_URL_CLIENT_QUERY_PROCESSOR_H
