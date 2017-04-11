#include "aligner.h"

#include <QtWidgets/QLayout>

#include <ui/common/accessor.h>
#include <ui/style/helper.h>
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
    m_skipInvisible(false),
    m_minimumWidth(style::Hints::kMinimumFormLabelWidth)
{
    if (parent)
    {
        installEventHandler(parent, { QEvent::Show, QEvent::LayoutRequest }, this,
            [this]()
            {
                if (!m_masterAligner)
                    align();
            });
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

void QnAligner::addAligner(QnAligner* aligner)
{
    if (aligner->m_masterAligner == this)
    {
        NX_ASSERT(m_aligners.contains(aligner));
        return;
    }

    NX_ASSERT(aligner->m_masterAligner.isNull());
    if (aligner->m_masterAligner)
        return;

    m_aligners << aligner;
    aligner->m_masterAligner = this;

    align();

    connect(aligner, &QObject::destroyed, this,
        [this, aligner]()
        {
            m_aligners.removeAll(aligner);
        });
}

void QnAligner::clear()
{
    m_widgets.clear();
    m_aligners.clear();
}

void QnAligner::registerTypeAccessor(const QLatin1String& className, AbstractAccessor* accessor)
{
    NX_ASSERT(!m_accessorByClassName.contains(className));
    m_accessorByClassName[className] = accessor;
}

int QnAligner::maxWidth() const
{
    int maxWidth = m_minimumWidth;
    const bool alignInvisible = !m_skipInvisible;

    for (auto w: m_widgets)
    {
        if (alignInvisible || w->isVisible())
            maxWidth = std::max(accessor(w)->get(w).toInt(), maxWidth);
    }

    for (auto a: m_aligners)
        maxWidth = std::max(a->maxWidth(), maxWidth);

    return maxWidth;
}

void QnAligner::enforceWidth(int width)
{
    const bool alignInvisible = !m_skipInvisible;
    for (auto w: m_widgets)
    {
        if (alignInvisible || w->isVisible())
            accessor(w)->set(w, width);
    }

    for (auto a: m_aligners)
        a->enforceWidth(width);

    if (auto parentWidget = qobject_cast<QWidget*>(parent()))
    {
        if (parentWidget->layout())
            parentWidget->layout()->activate();
    }
}

void QnAligner::align()
{
    if (m_widgets.isEmpty() && m_aligners.isEmpty())
        return;

    enforceWidth(maxWidth());
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

int QnAligner::minimumWidth() const
{
    return m_minimumWidth;
}

void QnAligner::setMinimumWidth(int value)
{
    if (m_minimumWidth == value)
        return;

    m_minimumWidth = value;
    align();
}
