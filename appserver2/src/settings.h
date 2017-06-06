/**********************************************************
* Aug 10, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_EC2_SETTINGS_H
#define NX_EC2_SETTINGS_H

#include <chrono>
#include <map>

#include <QString>
#include <QVariant>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>


namespace ec2 {

class Settings
{
public:
    bool dbReadOnly() const;

    //time_sync
    size_t internetSyncTimePeriodSec(size_t defaultValue) const;
    size_t maxInternetTimeSyncRetryPeriodSec(size_t defaultValue) const;

    void loadParams( std::map<QString, QVariant> confParams );

private:
    std::map<QString, QVariant> m_confParams;
    mutable QnMutex m_mutex;

    template<typename ValueType>
    ValueType value( const QString& name, ValueType defaultValue = ValueType() ) const
    {
        QnMutexLocker lk( &m_mutex );

        auto paramIter = m_confParams.find( name );
        if( paramIter == m_confParams.cend() )
            return defaultValue;
        return paramIter->second.value<ValueType>();
    }
};

}   //ec2

#endif  //NX_EC2_SETTINGS_H
