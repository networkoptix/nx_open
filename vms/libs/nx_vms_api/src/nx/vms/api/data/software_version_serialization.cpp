// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_version_serialization.h"

#include <array>

#include <nx/fusion/model_functions.h>

namespace nx::utils {

void serialize(const SoftwareVersion& value, QString* target)
{
    *target = value.toString();
}

bool deserialize(const QString& value, SoftwareVersion* target)
{
    return target->deserialize(value);
}

void serialize(const SoftwareVersion& value, QnUbjsonWriter<QByteArray>* stream)
{
    QnUbjson::serialize(
        std::array<int, 4>{value.major, value.minor, value.bugfix, value.build}, stream);
}

bool deserialize(QnUbjsonReader<QByteArray>* stream, SoftwareVersion* target)
{
    std::array<int, 4> segments;
    if (!QnUbjson::deserialize(stream, &segments))
        return false;

    target->major = segments[0];
    target->minor = segments[1];
    target->bugfix = segments[2];
    target->build = segments[3];
    return true;
}

void serialize(const SoftwareVersion& value, QnCsvStreamWriter<QByteArray>* target)
{
    target->writeField(value.toString());
}

QN_FUSION_DEFINE_FUNCTIONS(SoftwareVersion, (json_lexical)(xml_lexical))

} // namespace nx::utils
