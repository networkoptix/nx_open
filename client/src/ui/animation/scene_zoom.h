#ifndef scene_zoom_h1907
#define scene_zoom_h1907

#include "animation.h"
#include "core/resource/media_resource.h"


class GraphicsView;

class CLSceneZoom : public CLAnimation
{
	Q_OBJECT
    Q_PROPERTY(qreal zoom	READ getZoom WRITE setZoom)
public:
	CLSceneZoom(GraphicsView* gview);
	virtual ~CLSceneZoom();

	void zoom_delta(qreal delta, int duration, int delay, QPoint unmoved_point = QPoint(0,0), CLAnimationCurve curve =  SLOW_END_POW_40);
	void zoom_abs(qreal z, int duration, int delay, QPoint unmoved_point = QPoint(0,0), CLAnimationCurve curve =  SLOW_END_POW_40);
	void zoom_minimum(int duration, int delay, QPoint unmoved_point = QPoint(0,0), CLAnimationCurve curve =  SLOW_END_POW_40);

	qreal getZoom() const;
    void setZoom(qreal z);

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;



protected:
	void zoom_helper(int duration, int delay, CLAnimationCurve curve);

	void set_qulity_helper();

protected:
	qreal m_zoom, m_targetzoom;

	QnStreamQuality m_quality;

    QPoint m_unmoved_point; // during zoom this point should remain same position on the screen
};

#endif //scene_zoom_h1907
