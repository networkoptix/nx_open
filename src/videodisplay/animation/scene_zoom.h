#ifndef scene_zoom_h1907
#define scene_zoom_h1907

#include "abstract_animation.h"
#include <QPoint>

class GraphicsView;

class CLSceneZoom : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLSceneZoom(GraphicsView* gview);
	virtual ~CLSceneZoom();


	void zoom_delta(qreal delta);
	void zoom_abs(qreal z);
	void zoom_default();

	qreal getZoom() const;

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;


private slots:
	void onNewFrame(int frame);
	

protected:
	void zoom_helper();

protected:

	GraphicsView* m_view;
	qreal m_zoom,  m_targetzoom, m_diff, m_start_point;

};

#endif //scene_zoom_h1907