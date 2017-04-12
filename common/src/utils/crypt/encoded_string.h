#pragma once

#include <nx/fusion/model_functions_fwd.h>

class QnEncodedString
{
public:
    QnEncodedString();
    explicit QnEncodedString(const QString& value);
    static QnEncodedString fromEncoded(const QString& encoded);

    QString value() const;
    void setValue(const QString& value);

    QString encoded() const;
    void setEncoded(const QString& encoded);

    bool isEmpty() const;

    bool operator==(const QnEncodedString& value) const;
    bool operator!=(const QnEncodedString& value) const;

private:
    QString m_value;
};

QDataStream &operator<<(QDataStream &stream, const QnEncodedString &encodedString);
QDataStream &operator>>(QDataStream &stream, QnEncodedString &encodedString);

QDebug operator<<(QDebug dbg, const QnEncodedString& value);

QN_FUSION_DECLARE_FUNCTIONS(QnEncodedString, (metatype)(lexical)(json))
