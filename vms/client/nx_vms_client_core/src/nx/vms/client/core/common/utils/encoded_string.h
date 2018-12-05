#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::client::core {

class EncodedString
{
public:
    explicit EncodedString(const QByteArray& key = QByteArray());
    explicit EncodedString(const QString& value, const QByteArray& key = QByteArray());
    static EncodedString fromEncoded(const QString& encoded, const QByteArray& key = QByteArray());

    QString value() const;
    void setValue(const QString& value);

    QString encoded() const;
    void setEncoded(const QString& encoded);

    bool isEmpty() const;

    bool operator==(const EncodedString& value) const;
    bool operator!=(const EncodedString& value) const;

private:
    QByteArray m_key;
    QString m_value;
};

QDataStream &operator<<(QDataStream &stream, const EncodedString &encodedString);
QDataStream &operator>>(QDataStream &stream, EncodedString &encodedString);

QDebug operator<<(QDebug dbg, const EncodedString& value);

} // namespace nx::vms::client::core

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::client::core::EncodedString, (metatype)(lexical)(json))
