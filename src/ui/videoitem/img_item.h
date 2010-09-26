#ifndef img_item_h_2211
#define img_item_h_2211

#include "abstract_scene_item.h"
#include <QMutex>
#include <QTime>
#include <QFont>

class CLImageItem : public CLAbstractSceneItem
{
	Q_OBJECT
public:
	CLImageItem(GraphicsView* view, int max_width, int max_height,
		QString name="", QObject* handler=0);

	virtual int height() const;
	virtual int width() const;


	virtual float aspectRatio() const;
	int imageWidth() const;
	int imageHeight() const;

	void setInfoText(QString text);

	void setShowInfoText(bool show);
	void setShowImagesize(bool show);

	//====rotation=======
	void drawRotationHelper(bool val);
	void setRotationPoint(QPointF point1, QPointF point2);



signals:
	void onAspectRatioChanged(CLImageItem* item);

protected:
	virtual void drawStuff(QPainter* painter);
	virtual void drawInfoText(QPainter* painter);

	void drawRotationHelper(QPainter* painter);

	virtual bool wantText() const;

protected:
	mutable QMutex m_mutex;
	int m_imageWidth;
	int m_imageHeight;

	int m_imageWidth_old;
	int m_imageHeight_old;

	mutable QMutex m_mutex_aspect;
	float m_aspectratio;



	bool m_showinfotext;
	bool m_showimagesize;

	bool m_showing_text; // showing text now
	QTime m_textTime; // draw text is very very very expensive. so want to delay it a bit
	bool m_timeStarted;

	
	int m_max_width, m_max_height;
	QString m_infoText;

	//rotation
	bool m_draw_rotation_helper;
	QPointF m_rotation_center;
	QPointF m_rotation_hand;

	QFont m_Info_Font;


};


#endif //img_item_h_2211