#include "settings.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QCryptographicHash>

#include <nx/utils/range_adapters.h>

namespace nx::vms::server::analytics {

QnUuid calculateModelId(const api::analytics::SettingsModel& settingsModel)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QJsonDocument(settingsModel).toJson(QJsonDocument::Compact));
    return QnUuid::fromRfc4122(hash.result());
}

QJsonObject toJsonObject(const api::analytics::SettingsErrors& errors)
{
    QJsonObject result;

    for (const auto& [name, value]: nx::utils::constKeyValueRange(errors))
        result.insert(name, value);

    return result;
}

} // namespace nx::vms::server::analytics
