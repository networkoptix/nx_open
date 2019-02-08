#pragma once

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::client::core {

/**
 * Encapsulates cryptography logic, allowing to use this class as a struct field, hiding secure
 * (de)serialization behind the scenes. AES128 with a hardcoded key is used by default, but an
 * extra key can be assigned externally.
 *
 * Can be constructed from encoded or from decoded value. This makes difference when we are
 * changing cryptography key for an existing class instance. So it supports scenarios when class
 * is created from the decoded value, then key is set, and after that class is serialized to json.
 * Alternatively we can deserialize this class from json and only after that setup the correct key
 * to get the decoded value.
 *
 * Executing cryptography algorithms is lazy, called only when actually needed to avoid (de)cypher
 * before actual key is set.
 */
class EncodedString
{
public:
    /** Construct an empty value. Mode will be deducted on first setter call. */
    explicit EncodedString(const QByteArray& key = QByteArray());

    /** Create from decoded value. */
    static EncodedString fromDecoded(const QString& decoded, const QByteArray& key = QByteArray());

    /** Create from encoded value. */
    static EncodedString fromEncoded(const QString& encoded, const QByteArray& key = QByteArray());

    /** Extra cryptography key. */
    QByteArray key() const;

    /** Reset extra cryptography key. All calculated and cached values will be invalidated. */
    void setKey(const QByteArray& key);

    QString decoded() const;
    void setDecoded(const QString& value);

    QString encoded() const;
    void setEncoded(const QString& value);

    /** Checks if no value is set actually. */
    bool isEmpty() const;

    bool operator==(const EncodedString& value) const;
    bool operator!=(const EncodedString& value) const;

private:
    enum class Mode
    {
        empty,
        encoded,
        decoded
    };
    Mode m_mode = Mode::empty;
    QByteArray m_key;
    mutable std::optional<QString> m_encodedValue;
    mutable std::optional<QString> m_decodedValue;
};

QDataStream &operator<<(QDataStream& stream, const EncodedString& value);
QDataStream &operator>>(QDataStream& stream, EncodedString& value);
QDebug operator<<(QDebug dbg, const EncodedString& value);
void PrintTo(const EncodedString& value, ::std::ostream* os);

} // namespace nx::vms::client::core

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::client::core::EncodedString, (metatype)(lexical)(json))
