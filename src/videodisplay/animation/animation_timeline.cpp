#include "animation_timeline.h"
#include <QtCore/qmath.h>


static inline qreal qt_smoothBeginEndMixFactor(qreal value)
{
	return qMin(qMax(1 - value * 2 + qreal(0.3), qreal(0.0)), qreal(1.0));
}


static inline qreal ln_normal(qreal value)
{
	// assume that value [0;1];
	//value = value*3.0+1.0; //[1;4]
	
	static const qreal _2ln4 = 2.7725887222397812376689284858327;

	return qLn(value*3.0+1.0)/_2ln4 + qreal(0.5); // [0;ln4]/2ln4+0.5 => result [0.5; 1]

	
}

//===================================================

CLAnimationTimeLine::CLAnimationTimeLine(CLAnimationCurve curve , int duration , QObject *parent ):
QTimeLine(duration, parent ),
m_curve(curve)
{

}

CLAnimationTimeLine::~CLAnimationTimeLine()
{

}

qreal CLAnimationTimeLine::valueForTime ( int msec ) const
{
	switch(m_curve)
	{
	case CLAnimationCurve::SLOW_END:
		return slow_end(msec);
		
	case INHERITED:
	default:
		return QTimeLine::valueForTime(msec);
	    
	}
}

qreal CLAnimationTimeLine::slow_end( int msec ) const
{
		qreal value = msec / qreal(duration());

		const qreal lnprogress = ln_normal(value);
		const qreal linearProgress = value;
		const qreal mix = qt_smoothBeginEndMixFactor(value);
		return  linearProgress * mix + lnprogress* (1-mix) ;
}

