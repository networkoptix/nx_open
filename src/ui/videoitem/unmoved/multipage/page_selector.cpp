#include "page_selector.h"
#include "page_subitem.h"
#include "base/log.h"

static const int halfPagesPerPage = 4; // 1 2 3 4 curr 6 7 8 9

static const qreal normalOpacity = 0.5;
static const qreal activeOpacity = 1.0;
static const qreal itemsDistance = 25;

QnPageSelector::QnPageSelector(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity):
CLUnMovedInteractiveOpacityItem(name, parent, normal_opacity, active_opacity),
m_maxPage(1),
m_currPage(1)
{
    //setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
}

QnPageSelector::~QnPageSelector()
{
    stopAnimation();
}

void QnPageSelector::setMaxPageNumber(unsigned int maxpage)
{
    if (m_maxPage == maxpage)
        return;
    
    m_maxPage = maxpage;

    if (m_currPage > m_maxPage)
        m_currPage = m_maxPage;

    updateItems();
}

unsigned int QnPageSelector::getMaxPageNumber() const
{
    return m_maxPage;
}

QRectF QnPageSelector::boundingRect() const
{
    QList<QGraphicsItem*> lst = childItems();

    if (childItems().size()==0)
        return QRect(0,0,0,0);

    return QRect(0, 0, m_totalWidth, lst.at(0)->boundingRect().height());
}

unsigned int QnPageSelector::getCurrentPage() const
{
    return m_currPage;
}

void QnPageSelector::setCurrentPage(unsigned int page)
{
    m_currPage = qMax(page, (unsigned int)1);
    m_currPage = qMin(m_currPage, m_maxPage);


    updateItems();
}

void QnPageSelector::onPageSelected(int page)
{
    if (page == m_currPage)
        return;

    m_currPage = page;
    updateItems();

    emit onNewPageSlected(m_currPage);
}


void QnPageSelector::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //painter->fillRect(boundingRect(), QColor(255,0,0,128));
}


void QnPageSelector::updateItems()
{
    m_totalWidth = 0;

    QList<QGraphicsItem*> lst = childItems();
    foreach(QGraphicsItem* itm, lst)
    {
        disconnect((static_cast<QnPageSubItem*>(itm)), SIGNAL(onPageSelected(int)), this, SLOT(onPageSelected(int)));
        scene()->removeItem(itm);
        delete itm;
    }

    //=====================

    if (m_maxPage>2)// if we have at least 3 pages
    {
        addPrev();
    }

    int minPage = qMax( qMin(m_currPage - halfPagesPerPage, m_maxPage - 2 * halfPagesPerPage ), 1);
    int maxPage = qMin( qMax(m_currPage + halfPagesPerPage, 2 * halfPagesPerPage + 1), m_maxPage);


    if (minPage > 1)
    {
        // if we do not see link to the fist page 
        addEllipsis(true);
    }


    if (maxPage>1)
    for (int i = minPage; i <= maxPage; ++i)
    {
        addNumber(i);
    }


    if (maxPage < m_maxPage)
    {
        // if we do not see link to last page
        addEllipsis(false);
    }

    if (m_maxPage>2)// if we have at least 3 pages
    {
        addNext();
    }

    m_totalWidth -= itemsDistance;

}

//================================================================================

void QnPageSelector::addEllipsis(bool left)
{
    QnPageSubItem* item = new QnPageSubItem("...", this, normalOpacity, activeOpacity, left ? 1 : m_maxPage, false);
    addItemHelper(item);    
}

void QnPageSelector::addPrev()
{
    QnPageSubItem* item = new QnPageSubItem("Prev", this, normalOpacity, activeOpacity, m_currPage == 1 ? m_maxPage : m_currPage - 1, false );
    addItemHelper(item);
    m_totalWidth+=10;
}

void QnPageSelector::addNext()
{
    QnPageSubItem* item = new QnPageSubItem("Next", this, normalOpacity, activeOpacity, m_currPage == m_maxPage ? 1 : m_currPage + 1, false );
    addItemHelper(item);
}

void QnPageSelector::addNumber(int page)
{
    bool curr = (page == m_currPage);
    qreal no = curr ? activeOpacity : normalOpacity;
    QnPageSubItem* item = new QnPageSubItem(QString::number(page), this, no, activeOpacity, page, curr);
    addItemHelper(item);
}

void QnPageSelector::addItemHelper(QnPageSubItem* item)
{
    item->setPos(m_totalWidth, 0);
    m_totalWidth += item->boundingRect().width();
    m_totalWidth += itemsDistance;
    connect(item, SIGNAL(onPageSelected(int)), this, SLOT(onPageSelected(int)));
}