#ifndef cl_animation_timeline_h_0004
#define cl_animation_timeline_h_0004

 #include <QTimeLine>

class CLAnimationTimeLine : public QTimeLine
{
public:
	enum CLAnimationCurve 
	{
		INHERITED,// derived from QTimeLine
		SLOW_END, // slow end
		SLOW_END_POW_20,
		SLOW_END_POW_22,
		SLOW_END_POW_25,
		SLOW_END_POW_30,
		SLOW_END_POW_35,
		SLOW_END_POW_40,
		SLOW_START 
	};
	CLAnimationTimeLine(CLAnimationCurve curve = INHERITED, int duration = 1000, QObject *parent = 0);
	virtual ~CLAnimationTimeLine();

	void setCurve(CLAnimationCurve curve);
	virtual qreal valueForTime ( int msec ) const;

	//======================
	static void setAnimationSpeed(qreal speed);
	static qreal getAnimationSpeed();


protected:
	CLAnimationCurve m_curve;


	static qreal m_animation_speed;

private:
	qreal slow_end( int msec ) const;

	qreal slow_end_pow( int msec, qreal pw ) const;

	qreal slow_start( int msec ) const;
	
};


#endif //cl_animation_timeline_h_0004