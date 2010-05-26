#ifndef cl_animation_timeline_h_0004
#define cl_animation_timeline_h_0004

 #include <QTimeLine>

class CLAnimationTimeLine : public QTimeLine
{
public:
	enum CLAnimationCurve 
	{
		INHERITED,// derived from QTimeLine
		SLOW_END // slow end
//		SLOW_START 
	};
	CLAnimationTimeLine(CLAnimationCurve curve = INHERITED, int duration = 1000, QObject *parent = 0);
	virtual ~CLAnimationTimeLine();

	virtual qreal valueForTime ( int msec ) const;
protected:
	CLAnimationCurve m_curve;
private:
	qreal slow_end( int msec ) const;
	
};


#endif //cl_animation_timeline_h_0004