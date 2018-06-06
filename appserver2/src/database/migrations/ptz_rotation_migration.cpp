#include "ptz_rotation_migration.h"
#include "ptz_rotation_migration_structs.h"

#include <optional>

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
        convertedData.position = nx::core::ptz::PtzVector(
            record.data.position,
            nx::core::ptz::PtzVector::kPtzComponents);

        QnPtzPresetRecord convertedRecord;
        convertedRecord.preset = record.preset;
        convertedRecord.data = convertedData;

        newHash[itr.key()] = convertedRecord;
    }

    return QJson::serialized(newHash);
}

} // namespace

bool addRotationToPresets(QSqlDatabase& database)
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

        const auto convertedPresets = convertPresets(serializedPresets);
        if (convertedPresets == std::nullopt)
            return false;

        presetRecords[id] = *convertedPresets;
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

    return false;
}

} // namespace ptz
} // namespace migration
} // namespace ec2