#ifndef QN_INSTRUMENTED_H
#define QN_INSTRUMENTED_H

#include <QtGui/QGraphicsItem>

#include <utils/common/forward.h>

namespace detail {
    class InstrumentedBase {
    protected:
        InstrumentedBase();

        void updateScene(QGraphicsScene *scene, QGraphicsItem *item);

    private:
        QGraphicsScene *m_scene;
    };

} // namespace detail


/**
 * Base class for items that wish to be automatically registered with their 
 * scene's instrument managers.
 */
template<class Base>
class Instrumented: public Base, private detail::InstrumentedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Instrumented, Base, { updateScene(this->scene(), this); });

    virtual ~Instrumented() {
        updateScene(NULL, this);
    }

protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if(change == QGraphicsItem::ItemSceneHasChanged)
            updateScene(this->scene(), this);

        return Base::itemChange(change, value);
    }
};

#endif // QN_INSTRUMENTED_H
