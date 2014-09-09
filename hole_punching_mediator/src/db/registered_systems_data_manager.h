/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef REGISTERED_SYSTEMS_DATA_MANAGER_H
#define REGISTERED_SYSTEMS_DATA_MANAGER_H

#include <functional>

#include <QtCore/QString>


namespace nx_hpm
{
    class SystemRegistrationData
    {
    public:
        QString systemName;
        QString authenticationKey;
        //TODO add some security information: user key (if supplied), mediaserver guid, ipaddress, timestamp, something else
    };

    enum DbErrorCode
    {
        ok,
        notUnique,  //insert/update operation breaks uniqueness restriction
        ioError
    };

    /*!
        \note Contains cache inside, so read operations are done in sync mode (sonce no blocking operations are implied)
    */
    class RegisteredSystemsDataManager
    {
    public:
        bool saveRegistrationDataAsync( const SystemRegistrationData& registrationData, std::function<void(DbErrorCode)> );
        //!Get operation is done in sync mode, since it always hits im-memory cache
        bool findRegistrationData( const QString& systemName, SystemRegistrationData* const registrationData );
    };
}

#endif  //REGISTERED_SYSTEMS_DATA_MANAGER_H
