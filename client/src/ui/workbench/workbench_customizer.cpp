#include "workbench_customizer.h"

// --------------------------------------------------------------------------- //
// QnObjectStarCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarCustomizationSerializer(QObject *parent = NULL): 
        QObject(parent),
        QnJsonSerializer(QMetaType::QObjectStar)
    {
        m_serializerByType.insert(QMetaType::QObjectStar, this);
    }

protected:
    virtual void serializeInternal(const void *, QVariant *) const {
        assert(false); /* We should never get here. */
    }
    
    virtual bool deserializeInternal(const QVariant &value, void *target) const {
        QObject *object = *static_cast<QObject **>(target);

        QVariantMap map;
        if(!QJson::deserialize(value, &map))
            return false;

        
    }

private:
    QnJsonSerializer *serializer(int type) {
        QnJsonSerializer *result = m_serializerByType.value(type);
        if(!result) {
            /* QnJsonSerializer does locking, so we use local cache to avoid it. */
            result = QnJsonSerializer::forType(type); 
            m_serializerByType[type] = result;
        }
        return result;
    }

private:
    QnFlatMap<int, QnJsonSerializer *> m_serializerByType;
};


// --------------------------------------------------------------------------- //
// QnObjectStarStringHashCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarStringHashCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarStringHashCustomizationSerializer(): QnJsonRestHandler(qMetaTypeId())


private:
    
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

