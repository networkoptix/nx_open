#ifndef scene_zoom_h1907
#define scene_zoom_h1907

#include "abstract_animation.h"
#include <QPoint>
#include "streamreader\streamreader.h"

class GraphicsView;

class CLSceneZoom : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLSceneZoom(GraphicsView* gview);
	virtual ~CLSceneZoom();


	void zoom_delta(qreal delta, int duration, int delay = 0);
	void zoom_abs(qreal z, int duration, int delay = 0);
	void zoom_default(int duration, int delay = 0);
	void zoom_minimum(int duration, int delay = 0);

	qreal getZoom() const;

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;


private slots:
	void valueChanged( qreal dpos );
	

protected:
	void zoom_helper(int duration, int delay);

	void set_qulity_helper();

protected:

	GraphicsView* m_view;
	qreal m_zoom,  m_targetzoom, m_diff, m_start_point;

	CLStreamreader::StreamQuality m_quality;

};

#endif //scene_zoom_h1907