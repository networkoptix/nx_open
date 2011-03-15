#ifndef unmoved_pixture_button_h_1867
#define unmoved_pixture_button_h_1867

#include "unmoved_interactive_opacity_item.h"
class QPropertyAnimation;

class CLUnMovedPixture : public CLUnMovedInteractiveOpacityItem
{
	Q_OBJECT
public:
	CLUnMovedPixture(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z);
	~CLUnMovedPixture();
	QRectF boundingRect() const;

	void setMaxSize(int max_width, int max_height);

    void setFps(int fps);

	QSize getSize() const;

protected slots:
        void onTimer();


protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:

    int m_width;
    int m_height;
    qreal m_opacity;


    const QPixmap *m_CurrentImg;
    QList<QPixmap> m_Images;
    QTimer m_Timer;

    int m_CurrentIndex;
    bool m_forwardDirection;
};

//=============================================================================================

class CLUnMovedPixtureButton : public CLUnMovedPixture
{
	Q_OBJECT
public:
	CLUnMovedPixtureButton(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z);
	~CLUnMovedPixtureButton();
signals:
	void onPressed(QString);
protected:
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );

};

#endif //unmoved_pixture_button_h_1867