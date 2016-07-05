#include "shadow_shape_provider.h"
#include "shadow_item.h"

QnShadowShapeProvider::QnShadowShapeProvider():
    m_item(NULL)
{}

QnShadowShapeProvider::~QnShadowShapeProvider() {
    if(m_item) {
        m_item->setShapeProvider(NULL);
        m_item = NULL;
    }
}

QnShadowItem *QnShadowShapeProvider::shadowItem() const {
    return m_item;
}

void QnShadowShapeProvider::invalidateShadowShape() {
    if(m_item)
        m_item->invalidateShadowShape();
}