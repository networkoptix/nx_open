#include "image_correction_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ImageCorrectionData,
    (eq)(ubjson)(json)(xml)(csv_record),
    ImageCorrectionData_Fields)

QByteArray ImageCorrectionData::toByteArray() const
{
    return QByteArray::number(blackLevel)
        .append(';')
        .append(QByteArray::number(whiteLevel))
        .append(';')
        .append(QByteArray::number(gamma))
        .append(';')
        .append(enabled ? '1' : '0')
        .append((char)0);
}

ImageCorrectionData ImageCorrectionData::fromByteArray(const QByteArray& data)
{
    ImageCorrectionData result;
    QList<QByteArray> params = data.split(';');
    if (params.size() >= 4)
    {
        result.blackLevel = params[0].toDouble();
        result.whiteLevel = params[1].toDouble();
        result.gamma = params[2].toDouble();
        result.enabled = params[3].toInt();
    }
    return result;
}

} // namespace nx::vms::api

void serialize_field(const nx::vms::api::ImageCorrectionData& data, QVariant* target)
{
    serialize_field(data.toByteArray(), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::ImageCorrectionData* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    *target = nx::vms::api::ImageCorrectionData::fromByteArray(tmp);
}
