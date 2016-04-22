#include "aligner.h"

#include <ui/common/accessor.h>

namespace
{
    class DefaultWidgetAccessor: public AbstractAccessor
    {
    public:
        DefaultWidgetAccessor() {}

        virtual QVariant get(const QObject *object) const override
        {
            const QWidget* w = qobject_cast<const QWidget*>(object);
            if (!w)
                return 0;
            return w->sizeHint().width();
        }

        virtual void set(QObject *object, const QVariant &value) const override
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
    m_defaultAccessor(new DefaultWidgetAccessor())
{

}

QnAligner::~QnAligner()
{
    qDeleteAll(m_accessorByClassName);
}

void QnAligner::addWidget(QWidget* widget)
{
    m_widgets << widget;
}

void QnAligner::addWidgets(std::initializer_list<QWidget*> widgets)
{
    for (QWidget* w : widgets)
        m_widgets << w;
}

void QnAligner::registerTypeAccessor(const QLatin1String& className, AbstractAccessor* accessor)
{
    NX_ASSERT(!m_accessorByClassName.contains(className));
    m_accessorByClassName[className] = accessor;
}

void QnAligner::start()
{
    align();
}

void QnAligner::align()
{
    if (m_widgets.isEmpty())
        return;

    auto widest = std::max_element(m_widgets.cbegin(), m_widgets.cend(), [this](QWidget* left, QWidget* right)
    {
        return accessor(left)->get(left).toInt() < accessor(right)->get(right).toInt();
    });
    int width = accessor(*widest)->get(*widest).toInt();

    for (QWidget* w : m_widgets)
        accessor(w)->set(w, width);
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