#pragma once

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization_format.h>
#include <nx/utils/exception.h>
#include <rest/server/json_rest_result.h>

namespace nx::network::rest {

// TODO: Should probably be moved somewhere into nx::network::http.
static const http::StringType kFormContentType("application/x-www-form-urlencoded");
static const http::StringType kJsonContentType("application/json");
static const http::StringType kUbjsonContentType("application/ubjson");

class Params: public QMultiMap<QString, QString>
{
public:
    using Base = QMultiMap<QString, QString>;
    using Base::Base;

    Params(const QMultiMap<QString, QString>& map);
    Params(const QHash<QString, QString>& hash);

    template<typename T>
    void insert(const QString& key, const T& value) { Base::insert(key, ::toString(value)); }

    inline QString operator[](const QString& key) const { return value(key); }

    static Params fromUrlQuery(const QUrlQuery& query);
    static Params fromList(const QList<QPair<QString, QString>>& list);
    static Params fromJson(const QJsonObject& value);

    QUrlQuery toUrlQuery() const;
    QList<QPair<QString, QString>> toList() const;
    QJsonObject toJson() const;
};

struct Content
{
    http::StringType type;
    http::StringType body;

    std::optional<QJsonValue> parse() const;
};

class Exception: public nx::utils::Exception
{
public:
    const QnRestResult::ErrorDescriptor descriptor;

    template<typename... Args>
    Exception(QnRestResult::Error errorCode, Args... args);

    QString message() const override;
};

//--------------------------------------------------------------------------------------------------

inline void qStringListAdd(QStringList* /*list*/)
{
}

template<typename... Args>
void qStringListAdd(QStringList* list, QString arg, Args... args)
{
    list->append(std::move(arg));
    qStringListAdd(list, std::forward<Args>(args)...);
}

template<typename... Args>
QStringList qStringListInit(Args... args)
{
    QStringList argList;
    qStringListAdd(&argList, std::forward<Args>(args)...);
    return argList;
}

template<typename... Args>
Exception::Exception(QnRestResult::Error code, Args... args):
    descriptor({code, qStringListInit(std::forward<Args>(args)...)})
{
}

} // namespace nx::network::rest

Q_DECLARE_METATYPE(nx::network::rest::Params);
