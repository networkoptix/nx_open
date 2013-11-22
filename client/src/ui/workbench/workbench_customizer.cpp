#include "workbench_customizer.h"

// --------------------------------------------------------------------------- //
// QnObjectStarCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarCustomizationSerializer(): 
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
        const QMetaObject *metaObject = object->metaObject();

        QVariantMap map;
        if(!QJson::deserialize(value, &map))
            return false;

        for(QVariantMap::const_iterator pos = map.begin(); pos != map.end(); pos++) {
            const QString &key = pos.key();
            const QVariant &jsonValue = *pos;

            int index = metaObject->indexOfProperty(key.toLatin1());
            if(index != -1) {
                QMetaProperty p = metaObject->property(index);
                if (!p.isWritable())
                    return false;

                QVariant targetValue = p.read(object);
                QnJsonSerializer *serializer = this->serializer(targetValue.userType());
                if(!serializer)
                    return false;

                if(!serializer->deserialize(jsonValue, &targetValue))
                    return false;

                p.write(object, targetValue);
                continue;
            }

            QObject *child = findDirectChild(object, key);
            if(child != NULL) {
                if(!this->deserialize(jsonValue, &child))
                    return false;
                continue;
            }
            
            return false;
        }

        return true;
    }

private:
    static QObject *findDirectChild(QObject *parent, const QString &name) {
        foreach(QObject *child, parent->children())
            if(child->objectName() == name)
                return child;
        return NULL;
    }
    
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
class QnWeakObjectHashCustomizationSerializer: public QnJsonSerializer {
public:
    QnWeakObjectHashCustomizationSerializer(): 
        QnJsonSerializer(qMetaTypeId<QnWeakObjectHash>()),
        m_objectSerializer(new QnObjectStarCustomizationSerializer())
    {}

protected:
    virtual void serializeInternal(const void *, QVariant *) const {
        assert(false); /* We should never get here. */
    }

    virtual bool deserializeInternal(const QVariant &value, void *target) const {
        QnWeakObjectHash &hash = *static_cast<QnWeakObjectHash *>(target);

        QVariantMap map;
        if(!QJson::deserialize(value, &map))
            return false;

        for(QVariantMap::const_iterator pos = map.begin(); pos != map.end(); pos++) {
            const QString &key = pos.key();
            const QVariant &jsonValue = *pos;

            if(!hash.contains(key))
                return false;

            QObject *object = hash.value(key).data();
            if(!object)
                continue; /* Just skip it. This is not a deserialization error. */

            if(!m_objectSerializer->deserialize(jsonValue, &object))
                return false;
        }

        return true;
    }

private:
    QScopedPointer<QnJsonSerializer> m_objectSerializer;
};



// --------------------------------------------------------------------------- //
// QnWorkbenchCustomizer
// --------------------------------------------------------------------------- //
QnWorkbenchCustomizer::QnWorkbenchCustomizer(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_serializer(new QnWeakObjectHashCustomizationSerializer())
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

void QnWorkbenchCustomizer::customize(const QVariant &customization) {
    m_serializer->deserialize(customization, &m_objectByKey);
}

