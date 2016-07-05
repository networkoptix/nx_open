#include "aligner.h"

#include <ui/common/accessor.h>

namespace
{
    class WidgetSizeAccessor: public AbstractAccessor
    {
    public:
        WidgetSizeAccessor() {}

        virtual QVariant get(const QObject* object) const override
        {
            const QWidget* w = qobject_cast<const QWidget*>(object);
            if (!w)
                return 0;
            return w->sizeHint().width();
        }

        virtual void set(QObject* object, const QVariant& value) const override
        {
            QWidget* w = qobject_cast<QWidget*>(object);
            if (!w)
                return;
            w->setFixedWidth(value.toInt());
        }

    };



}

QnAligner::QnAligner(QObject* parent /*= nullptr*/):
    base_type(parent),
    m_defaultAccessor(new WidgetSizeAccessor())
{

}

QnAligner::~QnAligner()
{
    qDeleteAll(m_accessorByClassName);
}

void QnAligner::addWidget(QWidget* widget)
{
    m_widgets << widget;
    align();
}

void QnAligner::addWidgets(std::initializer_list<QWidget*> widgets)
{
    for (QWidget* w : widgets)
        m_widgets << w;
    align();
}

void QnAligner::registerTypeAccessor(const QLatin1String& className, AbstractAccessor* accessor)
{
    NX_ASSERT(!m_accessorByClassName.contains(className));
    m_accessorByClassName[className] = accessor;
}

void QnAligner::align()
{
    if (m_widgets.isEmpty())
        return;

    int maxWidth = 0;
    for (auto w : m_widgets)
        maxWidth = std::max(accessor(w)->get(w).toInt(), maxWidth);

    for (QWidget* w : m_widgets)
        accessor(w)->set(w, maxWidth);
}

AbstractAccessor* QnAligner::accessor(QWidget *widget) const
{
    const QMetaObject *metaObject = widget->metaObject();

    while (metaObject)
    {
        if (const auto result = m_accessorByClassName.value(QLatin1String(metaObject->className())))
            return result;
        metaObject = metaObject->superClass();
    }

    return m_defaultAccessor.data();
}