#include "aligner.h"

#include <ui/common/accessor.h>
#include <utils/common/event_processors.h>

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

} // unnamed namespace


QnAligner::QnAligner(QObject* parent /*= nullptr*/):
    base_type(parent),
    m_defaultAccessor(new WidgetSizeAccessor()),
    m_skipInvisible(false)
{
    if (parent)
    {
        auto signalizer = new QnSingleEventSignalizer(this);
        signalizer->setEventType(QEvent::LayoutRequest);
        connect(signalizer, &QnMultiEventSignalizer::activated, this, &QnAligner::align);
        parent->installEventFilter(signalizer);
    }
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
    for (auto widget : widgets)
        m_widgets << widget;

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
    const bool alignInvisible = !m_skipInvisible;

    for (auto w : m_widgets)
        if (alignInvisible || w->isVisible())
            maxWidth = std::max(accessor(w)->get(w).toInt(), maxWidth);

    for (QWidget* w : m_widgets)
        if (alignInvisible || w->isVisible())
            accessor(w)->set(w, maxWidth);
}

AbstractAccessor* QnAligner::accessor(QWidget* widget) const
{
    const QMetaObject* metaObject = widget->metaObject();

    while (metaObject)
    {
        if (const auto result = m_accessorByClassName.value(QLatin1String(metaObject->className())))
            return result;
        metaObject = metaObject->superClass();
    }

    return m_defaultAccessor.data();
}

bool QnAligner::skipInvisible() const
{
    return m_skipInvisible;
}

void QnAligner::setSkipInvisible(bool value)
{
    if (m_skipInvisible == value)
        return;

    m_skipInvisible = value;
    align();
}
