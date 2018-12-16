#include "ptz_rotation_migration.h"
#include "ptz_rotation_migration_structs.h"

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log.h>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>

namespace ec2 {
namespace migration {
namespace ptz {

namespace {

std::optional<QString> convertPresets(const QString& oldSerializedPresets)
{
    using namespace nx::core;

    bool success = false;
    const auto presets = QJson::deserialized<OldPtzPresetRecordHash>(
        oldSerializedPresets.toUtf8(), OldPtzPresetRecordHash(), &success);

    if (!success)
        return std::nullopt;

    QnPtzPresetRecordHash newHash;

    for (auto itr = presets.cbegin(); itr != presets.cend(); ++itr)
    {
        const auto record = itr.value();

        QnPtzPresetData convertedData;
        convertedData.space = record.data.space;
        convertedData.position = nx::core::ptz::Vector(
            record.data.position,
            nx::core::ptz::Vector::kPtzComponents);

        QnPtzPresetRecord convertedRecord;
        convertedRecord.preset = record.preset;
        convertedRecord.data = convertedData;

        newHash[itr.key()] = convertedRecord;
    }

    return QString::fromUtf8(QJson::serialized(newHash));
}

} // namespace

bool addRotationToPresets(const nx::utils::log::Tag& logTag, QSqlDatabase& database)
{
    static const QString kPresetQuery = lm("SELECT id, value FROM vms_kvpair WHERE name='%1'")
        .arg(kPresetsPropertyKey);

    std::map<QString, QString> presetRecords;

    QSqlQuery presetQuery(database);
    presetQuery.exec(kPresetQuery);

    while (presetQuery.next())
    {
        const auto id = presetQuery.value(0).toString();
        auto serializedPresets = presetQuery.value(1).toString();

        if (const auto convertedPresets = convertPresets(serializedPresets))
        {
            presetRecords[id] = *convertedPresets;
        }
        else
        {
            const auto defaultValue = QJson::serialized(QnPtzPresetRecordHash());
            presetRecords[id] = defaultValue;
            NX_WARNING(
                logTag,
                lm("Unable to deserialize a preset record while executing migration, "
                    "setting a default value (%1) to the property %2. The old value: %3")
                    .args(defaultValue, kPresetsPropertyKey, serializedPresets));
        }
    }

    QSqlQuery updatePresetsQuery(database);
    updatePresetsQuery.prepare(lit("UPDATE vms_kvpair SET value=:value WHERE id=:id"));

    for (const auto& entry: presetRecords)
    {
        const auto& id = entry.first;
        const auto& serializedPresets = entry.second;

        updatePresetsQuery.bindValue(lit(":id"), id);
        updatePresetsQuery.bindValue(lit(":value"), serializedPresets);

        if (!updatePresetsQuery.exec())
            return false;
    }

    return true;
}

} // namespace ptz
} // namespace migration
} // namespace ec2
