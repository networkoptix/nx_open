/**********************************************************
* Aug 10, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_EC2_SETTINGS_H
#define NX_EC2_SETTINGS_H

#include <map>

#include <QMutex>
#include <QString>
#include <QVariant>

#include <utils/common/singleton.h>


namespace ec2 {

class Settings
:
    public Singleton<Settings>
{
public:
    bool dbReadOnly() const;

    void loadParams( std::map<QString, QVariant> confParams );

private:
    std::map<QString, QVariant> m_confParams;
    mutable QMutex m_mutex;

    template<typename ValueType>
    ValueType value( const QString& name, ValueType defaultValue = ValueType() ) const
    {
        QMutexLocker lk( &m_mutex );

        auto paramIter = m_confParams.find( name );
        if( paramIter == m_confParams.cend() )
            return defaultValue;
        return paramIter->second.value<ValueType>();
    }
};

}   //ec2

#endif  //NX_EC2_SETTINGS_H
