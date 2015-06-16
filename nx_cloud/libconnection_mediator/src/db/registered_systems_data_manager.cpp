
#include "registered_systems_data_manager.h"


namespace nx_hpm
{
    bool RegisteredDomainsDataManager::saveDomainDataAsync(
        const DomainRegistrationData& registrationData,
        std::function<void(DbErrorCode)>&& completionHandler )
    {
        //TODO 
        return false;
    }

    //!
    /*!
        \note \a completionHandler is allowed to be invoked directly from this method (if get operation hit cache). TODO really?
    */
    bool RegisteredDomainsDataManager::getDomainData(
        const QString& domain,
        std::function<void(DbErrorCode, const DomainRegistrationData&)>&& completionHandler )
    {
        //TODO 
        return false;
    }
}
