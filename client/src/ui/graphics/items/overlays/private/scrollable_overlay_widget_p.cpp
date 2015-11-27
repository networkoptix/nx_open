#include "scrollable_overlay_widget_p.h"

#include <ui/graphics/items/controls/html_text_item.h>

#include <utils/common/scoped_value_rollback.h>

namespace {
    const int defaultOverlayWidth = 250;
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
{
    m_scrollArea->setContentWidget(m_contentWidget);
    m_scrollArea->setMinimumWidth(defaultOverlayWidth);
    m_scrollArea->setMaximumWidth(defaultOverlayWidth);
    m_scrollArea->setAlignment(Qt::AlignBottom | Qt::AlignRight);

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
}

QnScrollableOverlayWidgetPrivate::~QnScrollableOverlayWidgetPrivate()
{}

QnUuid QnScrollableOverlayWidgetPrivate::addItem( QGraphicsWidget *item ) {
    item->setParentItem(m_contentWidget);

    QnUuid id = QnUuid::createUuid();
    m_items[id] = item;

    QObject::connect(item, &QGraphicsWidget::geometryChanged, m_contentWidget, [this]() {
        updatePositions(); //dirty hack to make graphics labels with pixmap caching work well
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
    q->update();
}

int QnScrollableOverlayWidgetPrivate::overlayWidth() const {
    return m_scrollArea->minimumWidth();
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
