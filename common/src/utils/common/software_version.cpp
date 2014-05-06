#include "software_version.h"

#include <algorithm>

#include <QtCore/QStringList>
#include <QtCore/QDataStream>

#include <utils/serialization/binary_functions.h>
#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>
#include <utils/serialization/csv_functions.h>

QString QnSoftwareVersion::toString(QnSoftwareVersion::Format format) const {
    QString result = QString::number(m_data[0]);
    for(int i = 1; i < format; i++)
        result += QLatin1Char('.') + QString::number(m_data[i]);
    return result;
}

bool operator<(const QnSoftwareVersion &l, const QnSoftwareVersion &r) {
    return std::lexicographical_compare(l.m_data.begin(), l.m_data.end(), r.m_data.begin(), r.m_data.end());
}

bool operator==(const QnSoftwareVersion &l, const QnSoftwareVersion &r) {
    return std::equal(l.m_data.begin(), l.m_data.end(), r.m_data.begin());
}

void serialize(const QnSoftwareVersion &value, QString *target) {
    *target = value.toString();
}

bool deserialize(const QString &value, QnSoftwareVersion *target) {
    /* Implementation differs a little bit from QnLexical conventions.
     * We try to set target to some sane value regardless of whether 
     * deserialization has failed or not. We also support OpenGL-style
     * extended versions. */

    std::fill(target->m_data.begin(), target->m_data.end(), 0);

    QString s = value.trimmed();
    int index = s.indexOf(L' ');
    if(index != -1)
        s = s.mid(0, index);

    bool result = !s.isEmpty();

    QStringList versionList = s.split(QLatin1Char('.'));
    for(int i = 0, count = qMin(4, versionList.size()); i < count; i++)
        result &= QnLexical::deserialize(versionList[i], &target->m_data[i]);

    return result;
}

QDataStream &operator<<(QDataStream &stream, const QnSoftwareVersion &version) {
    for(int i = 0; i < 4; i++)
        stream << version.m_data[i];
    return stream;
}

QDataStream &operator>>(QDataStream &stream, QnSoftwareVersion &version) {
    for(int i = 0; i < 4; i++)
        stream >> version.m_data[i];
    return stream;
}

QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnSoftwareVersion)

void serialize(const QnSoftwareVersion &value, QnOutputBinaryStream<QByteArray> *stream) {
    QnBinary::serialize(value.m_data, stream);
}

bool deserialize(QnInputBinaryStream<QByteArray> *stream, QnSoftwareVersion *target) {
    return QnBinary::deserialize(stream, &target->m_data);
}

void serialize_field(const QnSoftwareVersion &value, QnCsvStreamWriter<QByteArray> *target) {
    target->writeField(value.toString());
}
