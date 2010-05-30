#include "animation_timeline.h"
#include <QtCore/qmath.h>

qreal CLAnimationTimeLine::m_animation_speed = 1.0;

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

static inline qreal exp_normal(qreal value)
{
	// assume that value [0;1];
	value = (value-1.0/3)*3.0;//[-1;2]

	static const qreal exp_1 = exp(-1.0); 
	static const qreal exp_3 = exp(3.0); 
	static const qreal exp_3_m_1 = exp_3 - exp_1; 


	value = exp(value)-exp_1; // [0; exp_m_3-exp_m_1]

	
	return value/exp_3_m_1; //[0;0.5]
}

//===================================================

CLAnimationTimeLine::CLAnimationTimeLine(CLAnimationCurve curve , int duration , QObject *parent ):
QTimeLine(duration/m_animation_speed, parent ),
m_curve(curve)
{
	for (qreal i = 0; i < 1.1; i+=0.1)
	{
		qreal val = exp_normal(i);
		i = i;
	}
}

CLAnimationTimeLine::~CLAnimationTimeLine()
{
	stop();
}

void CLAnimationTimeLine::setCurve(CLAnimationCurve curve)
{
	m_curve = curve;
}

qreal CLAnimationTimeLine::valueForTime ( int msec ) const
{
	switch(m_curve)
	{
	case CLAnimationCurve::SLOW_END:
		return slow_end(msec);

	case CLAnimationCurve::SLOW_START:
		return slow_start(msec);
		
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

qreal CLAnimationTimeLine::slow_start( int msec ) const
{
	qreal value = msec / qreal(duration());

	const qreal expprogress = exp_normal(value);
	const qreal linearProgress = value;
	const qreal mix = qt_smoothBeginEndMixFactor(value);
	return   expprogress * mix + linearProgress* (1-mix) ;
}

//=================================================================
void CLAnimationTimeLine::setAnimationSpeed(qreal speed)
{
	m_animation_speed = speed;
}

qreal CLAnimationTimeLine::getAnimationSpeed()
{
	return m_animation_speed;
}