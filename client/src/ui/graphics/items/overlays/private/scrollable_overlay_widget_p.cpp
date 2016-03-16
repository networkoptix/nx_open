#include "scrollable_overlay_widget_p.h"

#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/math.h>

namespace {
    const int layoutSpacing = 1;
}

QnScrollableOverlayWidgetPrivate::QnScrollableOverlayWidgetPrivate(Qt::Alignment alignment, QnScrollableOverlayWidget *parent )
    : q_ptr(parent)
    , m_contentWidget(new QGraphicsWidget(parent))
    , m_scrollArea(new QnGraphicsScrollArea(parent))
    , m_mainLayout(new QGraphicsLinearLayout(Qt::Horizontal))
    , m_items()
    , m_alignment(alignment)
    , m_updating(false)
    , m_maxFillCoeff(1.0, 1.0)
{
    m_scrollArea->setContentWidget(m_contentWidget);
    m_scrollArea->setAlignment(Qt::AlignBottom | Qt::AlignRight);
    m_scrollArea->setProperty(Qn::NoBlockMotionSelection, true);

    m_contentWidget->setProperty(Qn::NoBlockMotionSelection, true);

    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_mainLayout->addItem(m_scrollArea);

    if (alignment.testFlag(Qt::AlignLeft)) {
        m_mainLayout->addStretch();
    } else if (alignment.testFlag(Qt::AlignRight)) {
        m_mainLayout->insertStretch(0);
    } else /* center */ {
        m_mainLayout->insertStretch(0);
        m_mainLayout->addStretch();
    }

    QObject::connect(m_scrollArea, &QGraphicsWidget::geometryChanged, m_scrollArea, [this]() {
        updatePositions();
    });
}

QnScrollableOverlayWidgetPrivate::~QnScrollableOverlayWidgetPrivate()
{}

QnUuid QnScrollableOverlayWidgetPrivate::addItem( QGraphicsWidget *item, const QnUuid &externalId ) {
    item->setParent(m_contentWidget);
    item->setParentItem(m_contentWidget);

    QnUuid id = externalId.isNull()
        ? QnUuid::createUuid()
        : externalId;
    m_items[id] = item;

    QObject::connect(item, &QGraphicsWidget::geometryChanged, m_contentWidget, [this]() {
        updatePositions();
    });

    updatePositions();
    return id;
}

void QnScrollableOverlayWidgetPrivate::removeItem( const QnUuid &id ) {
    if (QGraphicsWidget* item = m_items.take(id)) {
        delete item;
        updatePositions();
    }
}

void QnScrollableOverlayWidgetPrivate::clear() {
    for (QGraphicsWidget* item: m_items)
        delete item;
    m_items.clear();
    updatePositions();
}

void QnScrollableOverlayWidgetPrivate::updatePositions() {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    int height = 0;
    int widht = overlayWidth();
    for (QGraphicsWidget* item: m_items)
    {
        int left;
        if (m_alignment.testFlag(Qt::AlignLeft))
            left = 0;
        else if (m_alignment.testFlag(Qt::AlignRight))
            left = widht - item->size().width();
        else /* center */
            left = (widht - item->size().width()) / 2;

        item->setPos(left, height);
        height += item->size().height() + layoutSpacing;
    }

    m_contentWidget->resize(widht, std::max(0, height - layoutSpacing));

    Q_Q(QnScrollableOverlayWidget);
    q->updateGeometry();
}

int QnScrollableOverlayWidgetPrivate::overlayWidth() const {
    return m_scrollArea->geometry().width();
}

void QnScrollableOverlayWidgetPrivate::setOverlayWidth( int width ) {
    if (width == overlayWidth())
        return;

    if (width < overlayWidth()) {
        m_scrollArea->setMinimumWidth(width);
        m_scrollArea->setMaximumWidth(width);
    } else {
        m_scrollArea->setMaximumWidth(width);
        m_scrollArea->setMinimumWidth(width);
    }

    updatePositions();
}

QSizeF QnScrollableOverlayWidgetPrivate::minimumSize() const {
    if (m_items.isEmpty())
        return QSizeF();

    const auto widest = std::max_element(m_items.cbegin(), m_items.cend(), [](QGraphicsWidget* left, QGraphicsWidget* right) {
        return left->size().width() < right->size().width();
    });

    const auto tallest = std::max_element(m_items.cbegin(), m_items.cend(), [](QGraphicsWidget* left, QGraphicsWidget* right) {
        return left->size().height() < right->size().height();
    });

    Q_Q(const QnScrollableOverlayWidget);
    auto margins = q->contentsMargins();

    qreal maxWidth  = widest.value()->size().width()   + margins.left() + margins.right();
    qreal maxHeight = tallest.value()->size().height() + margins.top()  + margins.bottom();

    return QSizeF(maxWidth / m_maxFillCoeff.width(), maxHeight / m_maxFillCoeff.height());
}

QSizeF QnScrollableOverlayWidgetPrivate::maxFillCoeff() const {
    return m_maxFillCoeff;
}

void QnScrollableOverlayWidgetPrivate::setMaxFillCoeff( const QSizeF &coeff ) {
    if (qFuzzyEquals(m_maxFillCoeff, coeff))
        return;

    NX_ASSERT(qBetween(0.01, coeff.width(), 1.001) && qBetween(0.01, coeff.height(), 1.001), Q_FUNC_INFO, "Invalid values");

    m_maxFillCoeff = coeff;

    Q_Q(QnScrollableOverlayWidget);
    q->updateGeometry();
}
