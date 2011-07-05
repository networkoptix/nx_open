#ifndef unmoved_multu_page_selector_1718
#define unmoved_multu_page_selector_1718


#include "../unmoved_interactive_opacity_item.h"
class QPropertyAnimation;
class QnPageSubItem;

class QnPageSelector : public CLUnMovedInteractiveOpacityItem
{
	Q_OBJECT
signals:
    void onNewPageSlected(int page);
public:
	QnPageSelector(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
	~QnPageSelector();

    void setMaxPageNumber(int maxpage);
    unsigned int getMaxPageNumber() const;

    unsigned int getCurrentPage() const;
    void setCurrentPage(unsigned int page);

    QRectF boundingRect() const;

protected slots:
    void onPageSelected(int page);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    void updateItems();

    void addEllipsis(bool left);
    void addPrev();
    void addNext();
    void addNumber(int page);

    void addItemHelper(QnPageSubItem* item);

private:
    int m_currPage;
    int m_maxPage;

    int m_totalWidth;

};

//=============================================================================================


#endif //unmoved_multu_page_selector_1718
