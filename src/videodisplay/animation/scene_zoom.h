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


	void zoom(int delta);
	int getZoom() const;
private slots:
	void onNewFrame(int frame);
	

protected:
	

protected:

	QGraphicsView* m_view;
	int m_zoom;
	int m_targetzoom, m_diff, m_start_point;

};

#endif //scene_zoom_h1907