#include "customizer.h"

#include <utils/common/warnings.h>

// -------------------------------------------------------------------------- //
// QnCustomizer
// -------------------------------------------------------------------------- //
QnCustomizer::QnCustomizer(QObject *parent):
    QObject(parent)
{}

QnCustomizer::~QnCustomizer() {
    return;
}

void QnCustomizer::setCustomization(const QVariantMap &customization) {
    m_customization = customization;
}

const QVariantMap &QnCustomizer::customization() const {
    return m_customization;
}

void QnCustomizer::customize(QObject *object) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    QVarLengthArray<QMetaObject *, 32> metaObjects;

    const QMetaObject *metaObject = object->metaObject();
    while(metaObject) {
        metaObjects.append(metaObject);
        metaObject = metaObject->superClass();
    }

    for(int i = metaObjects.size() - 1; i >= 0; i--)
        customize(object, QLatin1String(metaObject->className()));
}

void QnCustomizer::customize(QObject *object, const QString &key) {
    QVariantMap::const_iterator pos = m_customization.find(key);
    if(pos == m_customization.end())
        continue;
    
    

}
