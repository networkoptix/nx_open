#include "workbench_customizer.h"

// --------------------------------------------------------------------------- //
// QnObjectStarCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarCustomizationSerializer(QObject *parent = NULL): 
        QObject(parent),
        QnJsonSerializer(QMetaType::QObjectStar)
    {}

protected:
    virtual void serializeInternal(const void *value, QVariant *target) const = 0;
    virtual bool deserializeInternal(const QVariant &value, void *target) const = 0;

private:

};


// --------------------------------------------------------------------------- //
// QnObjectStarStringHashCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarStringHashCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarStringHashCustomizationSerializer(): QnJsonRestHandler(qMetaTypeId())


private:
    QnFlatMap<int, QnJsonSerializer *> m_serializerByType;
};



// --------------------------------------------------------------------------- //
// QnWorkbenchCustomizer
// --------------------------------------------------------------------------- //
QnWorkbenchCustomizer::QnWorkbenchCustomizer(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{}

QnWorkbenchCustomizer::~QnWorkbenchCustomizer() {
    return;
}

void QnWorkbenchCustomizer::registerObject(const QString &key, QObject *object) {
    m_objectByKey.insert(key, object);
}

void QnWorkbenchCustomizer::unregisterObject(const QString &key) {
    m_objectByKey.remove(key);
}

void QnWorkbenchCustomizer::customize(const QVariant &cusomization) {
    
}

QnJsonSerializer *QnWorkbenchCustomizer::serializer(int type) {
    QnJsonSerializer *result = m_serializerByType.value(type);
    if(!result) {
        /* QnJsonSerializer does locking, so we use local cache to avoid it. */
        result = QnJsonSerializer::forType(type); 
        m_serializerByType[type] = result;
    }
    return result;
}
