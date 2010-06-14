#ifndef scene_zoom_h1907
#define scene_zoom_h1907

#include "abstract_animation.h"
#include <QPoint>

class QGraphicsView;

class CLSceneZoom : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLSceneZoom(QGraphicsView* gview);
	virtual ~CLSceneZoom();


	void zoom_delta(qreal delta);
	void zoom_abs(qreal z);
	void zoom_default();



	qreal getZoom() const;
private slots:
	void onNewFrame(int frame);
	

protected:
	void zoom_helper();

protected:

	QGraphicsView* m_view;
	qreal m_zoom,  m_targetzoom, m_diff, m_start_point;

};

#endif //scene_zoom_h1907