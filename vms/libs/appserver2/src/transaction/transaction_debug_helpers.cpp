#include "transaction_debug_helpers.h"
#include "amend_transaction_data.h"
#include "transaction_descriptor.h"

#include <nx/utils/url.h>

namespace ec2 {

QString debugString(const nx::vms::api::StorageData& data)
{
    const auto storageUrl = nx::utils::Url(data.url);
    auto storagePassword = storageUrl.password();
    if (!storagePassword.isNull())
    {
        if (storagePassword != kHiddenPasswordFiller)
            storagePassword = "Not null";
    }
    else
    {
        storagePassword = "null";
    }

    return QString("StorageData: id: %1, url: %2, password: %3")
        .arg(data.id.toString())
        .arg(storageUrl.toString(QUrl::RemovePassword))
        .arg(storagePassword);
}

} // namespace ec2
