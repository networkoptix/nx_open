#ifndef _API_PB_SERIALIZER_H_
#define _API_PB_SERIALIZER_H_

#include "serializer.h"

/**
 * Serialize resource to protobuf
 */
class QnApiPbSerializer : public QnApiSerializer {
    Q_DECLARE_TR_FUNCTIONS(QnApiPbSerializer)
public:
    const char* format() const { return "pb"; }

    void deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) override {}
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) override {}
    void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data) override {}
    void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data) override {}
};

#endif // _API_PB_SERIALIZER_H_
