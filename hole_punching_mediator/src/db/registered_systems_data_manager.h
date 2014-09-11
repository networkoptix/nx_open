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
    class DomainRegistrationData
    {
    public:
        //!Any token
        QString domain;
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
        \note Contains cache inside, so read operations can be completed immediately
    */
    class RegisteredDomainsDataManager
    {
    public:
        bool saveDomainDataAsync( const DomainRegistrationData& registrationData, std::function<void(DbErrorCode)>&& completionHandler );
        //!
        /*!
            \note \a completionHandler is allowed to be invoked directly from this method (if get operation hit cache). TODO really?
            \note This method is reentrant
        */
        bool getDomainData( const QString& domain, std::function<void(DbErrorCode, const DomainRegistrationData&)>&& completionHandler );

        static DomainRegistrationData* instance();
    };
}

#endif  //REGISTERED_SYSTEMS_DATA_MANAGER_H
