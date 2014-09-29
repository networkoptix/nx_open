/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#include "request_params.h"

#include <QtCore/QUrlQuery>

#include <nx_ec/data/api_connection_data.h>
#include <nx_ec/data/api_stored_file_data.h>
#include <utils/serialization/lexical_functions.h>


namespace ec2
{
    template<class T>
    bool deserialize(const QnRequestParamList &value, const QString &key, T *target) {
        auto pos = value.find(key);
        if(pos == value.end()) {
            return false;
        } else {
            return QnLexical::deserialize(pos->second, target);
        }
    }

    template<class T>
    void serialize(const T &value, const QString &key, QUrlQuery *target) {
        target->addQueryItem(key, QnLexical::serialized(value));
    }


    bool parseHttpRequestParams(const QString& command, const QnRequestParamList &params, QString *value) {
        assert(command == "getHelp");
        return deserialize(params, lit("group"), value);
    }

    bool parseHttpRequestParams(const QString& command, const QnRequestParamList &params, ApiStoredFilePath *value) {
        assert(command != "getHelp");
        return deserialize(params, lit("folder"), &(value->path)); // ApiStoredFilePath
    }

    void toUrlParams(const ApiStoredFilePath &name, QUrlQuery *query) {
        serialize(name.path, lit("folder"), query);
    }


    bool parseHttpRequestParams(const QString& command, const QnRequestParamList &params, QnUuid *id) {
        Q_UNUSED(command);
        return deserialize(params, lit("id"), id);
    }

    void toUrlParams(const QnUuid &id, QUrlQuery *query) {
        serialize(id, lit("id"), query);
    }


    bool parseHttpRequestParams(const QString& command, const QnRequestParamList &params, Qn::SerializationFormat *format) {
        Q_UNUSED(command);
        return deserialize(params, lit("format"), format);
    }

    void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery *query) {
        serialize(format, lit("format"), query);
    }


    bool parseHttpRequestParams(const QString& command, const QnRequestParamList& params, ApiLoginData* data)
    {
        Q_UNUSED(command);
        return 
            deserialize(params, lit("login"), &data->login) &&
            deserialize(params, lit("digest"), &data->passwordHash);
    }

    void toUrlParams(const ApiLoginData& data, QUrlQuery* query) 
    {
        serialize( data.login, lit("login"), query );
        serialize( data.passwordHash, lit("digest"), query );
    }

    bool parseHttpRequestParams(const QString&, const QnRequestParamList &, std::nullptr_t *) { return true; }
    void toUrlParams(const std::nullptr_t &, QUrlQuery *) {}

}
