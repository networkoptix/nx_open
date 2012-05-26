#ifndef QN_VIEWPORT_GEOMETRY_ACCESSOR_H
#define QN_VIEWPORT_GEOMETRY_ACCESSOR_H

#include <ui/common/accessor.h>

class ViewportGeometryAccessor: public AbstractAccessor {
public:
    virtual QVariant get(const QObject *object) const override;

    virtual void set(QObject *object, const QVariant &value) const override;
};

#endif // QN_VIEWPORT_GEOMETRY_ACCESSOR_H
