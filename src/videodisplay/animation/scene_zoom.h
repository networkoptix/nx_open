#ifndef scene_zoom_h1907
#define scene_zoom_h1907

#include "animation_timeline.h"
#include <QPoint>

class QGraphicsView;

class CLSceneZoom: public QObject
{
	Q_OBJECT
public:
	CLSceneZoom(QGraphicsView* gview);
	virtual ~CLSceneZoom();

	void stop();

	void zoom(int delta);
	int getZoom() const;
private slots:
		void onNewPos(int frame);
	

protected:
	

protected:

	QGraphicsView* m_view;
	CLAnimationTimeLine m_timeline;
	bool m_zooming_in; // zooming in or zooming out a the moment.
	int m_zoom;
	int m_targetzoom, m_diff, m_start_point;

	

};

#endif //scene_zoom_h1907