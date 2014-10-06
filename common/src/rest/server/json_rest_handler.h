#ifndef QN_JSON_REST_HANDLER_H
#define QN_JSON_REST_HANDLER_H

#include "request_handler.h"
#include "json_rest_result.h"

#include <utils/serialization/lexical_functions.h>

// TODO: #MSAPI 
// 
// Step1:
// rename to something =). I suggest QnBasicRestHandler.
// 
// Step2:
// In get/post routines deserialize passed format. 
// Construct QnRestResult (ex-QnJsonRestResult) with the given format and pass it into the actual
// get implementation. See json_rest_result.h for more. 
// 
// Step2.1:
// You may end up with some of the result types not being (de)serializable to/from
// our binary format. Make sure you've generated all the serializers. See 
// api_model_functions.cpp for more.
// 
// Step2.2:
// And you 
// 
// Step3:
// Think of inheriting ec2::BaseQueryHttpHandler from this one to share
// the code. I think it is quite doable.
// 
// Step4: 
// Inherit all MS API handlers from this one, with the exception
// of the ones that use hand-rolled serialization. The only such handlers that
// I'm aware of right now are QnLogRestHandler and QnRecordedChunksRestHandler.
// 
// Step5:
// Make sure QnRecordedChunksRestHandler adheres to the format interface.
// E.g. it also has parameter 'format' and handles the same values for it,
// and maybe some other values.
// 
// Step6:
// 
//

class QnJsonRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnJsonRestHandler();
    virtual ~QnJsonRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*);
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*);

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray& srcBodyContentType, QByteArray &result, 
                            QByteArray &contentType, const QnRestConnectionProcessor*) override;

    template<class T>
    bool requireParameter(const QnRequestParams &params, const QString &key, QnJsonRestResult &result, T *value, bool optional = false) const {
        auto pos = params.find(key);
        if(pos == params.end()) {
            if(optional) {
                return true;
            } else {
                result.setError(QnJsonRestResult::MissingParameter, lit("Parameter '%1' is missing.").arg(key));
                return false;
            }
        }

        if(!QnLexical::deserialize(*pos, value)) {
            result.setError(QnJsonRestResult::InvalidParameter, lit("Parameter '%1' has invalid value '%2'. Expected a value of type '%3'.").arg(key).arg(*pos).arg(QLatin1String(typeid(T).name())));
            return false;
        }

        return true;
    }

private:
    QnRequestParams processParams(const QnRequestParamList &params) const;

private:
    QByteArray m_contentType;
};


#endif // QN_JSON_REST_HANDLER_H
