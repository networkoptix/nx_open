#include "animation_timeline.h"

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
	case SLOW_END:
		return slow_end(msec);

	case SLOW_END_POW_20:
		return slow_end_pow(msec, 0.20);

	case SLOW_END_POW_22:
		return slow_end_pow(msec, 0.22);

	case SLOW_END_POW_25:
		return slow_end_pow(msec, 0.25);
	case SLOW_END_POW_30:
		return slow_end_pow(msec, 0.30);

	case SLOW_END_POW_35:
		return slow_end_pow(msec, 0.35);

	case SLOW_END_POW_40:
		return slow_end_pow(msec, 0.40);


	case SLOW_START:
		return slow_start(msec);

	case SLOW_START_SLOW_END:
		return slow_start_slow_end(msec);

	case INOUTBACK:
		return inOutBack(msec);

    case OUTCUBIC:
        return outCubic(msec);


		
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

qreal CLAnimationTimeLine::slow_start_slow_end( int msec ) const
{
	static QEasingCurve ec(QEasingCurve::InOutQuad);

	qreal value = msec / qreal(duration());
	return ec.valueForProgress(value);
}

qreal CLAnimationTimeLine::inOutBack( int msec ) const
{
	static QEasingCurve ec(QEasingCurve::InOutBack);

	qreal value = msec / qreal(duration());
	return ec.valueForProgress(value);
}

qreal CLAnimationTimeLine::slow_end_pow( int msec, qreal pw ) const
{
	qreal value = msec / qreal(duration());
	return pow(value, pw);
}

qreal CLAnimationTimeLine::outCubic(int msec) const
{
    static QEasingCurve ec(QEasingCurve::OutCubic);

    qreal value = msec / qreal(duration());
    return ec.valueForProgress(value);
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