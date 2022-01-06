// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_version.h"

#include <QtCore/QStringList>
#include <QtCore/QDataStream>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

void serialize(const SoftwareVersion& value, QString* target)
{
    *target = value.toString();
}

bool deserialize(const QString& value, SoftwareVersion* target)
{
    /* Implementation differs a little bit from QnLexical conventions.
     * We try to set target to some sane value regardless of whether
     * deserialization has failed or not. We also support OpenGL-style
     * extended versions. */

    std::fill(target->m_data.begin(), target->m_data.end(), 0);

    QString s = value.trimmed();
    auto index = s.indexOf(' ');
    if(index != -1)
        s = s.mid(0, index);

    bool result = !s.isEmpty();

    QStringList versionList = s.split(QLatin1Char('.'));
    for(int i = 0, count = qMin(4, versionList.size()); i < count; i++)
        result &= QnLexical::deserialize(versionList[i], &target->m_data[i]);

    return result;
}

void serialize(const SoftwareVersion& value, QnUbjsonWriter<QByteArray>* stream)
{
    QnUbjson::serialize(value.m_data, stream);
}

bool deserialize(QnUbjsonReader<QByteArray>* stream, SoftwareVersion* target)
{
    return QnUbjson::deserialize(stream, &target->m_data);
}

void serialize(const SoftwareVersion& value, QnCsvStreamWriter<QByteArray>* target)
{
    target->writeField(value.toString());
}

QDataStream& operator<<(QDataStream& stream, const SoftwareVersion& version)
{
    for (const auto value: version.m_data)
        stream << value;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, SoftwareVersion& version)
{
    for (auto& valueRef: version.m_data)
        stream >> valueRef;

    return stream;
}

QN_FUSION_DEFINE_FUNCTIONS(SoftwareVersion, (json_lexical)(xml_lexical))

} // namespace nx::vms::api

size_t qHash(const nx::vms::api::SoftwareVersion& softwareVersion)
{
    return softwareVersion.major() ^ softwareVersion.minor()
        ^ softwareVersion.bugfix() ^ softwareVersion.build();
}
