#ifndef QN_JSON_CONTEXT_H
#define QN_JSON_CONTEXT_H

#include "flat_map.h"
#include "json_serializer.h"

#include <utils/serialization/serialization.h>

class QnJsonSerializer;

namespace QJsonDetail { class ContextAccess; }


class QnJsonContext: public Qss::Context<QJsonValue> {
public:
    void registerSerializer(QnJsonSerializer *serializer) {
        m_serializerByType.insert(serializer->type(), serializer);
    }

private:
    friend class QJsonDetail::ContextAccess;
    QnFlatMap<unsigned int, QnJsonSerializer *> m_serializerByType;
};


namespace QJsonDetail {
    class ContextAccess {
    public:
        static QnJsonSerializer *serializer(QnJsonContext *ctx, int type) {
            return ctx->m_serializerByType.value(type);
        }
    };

} // namespace QJsonDetail


#endif // QN_JSON_CONTEXT_H
