#include "animation_timeline.h"


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
static qreal slow_end( qreal value ) 
{
	const qreal lnprogress = ln_normal(value);
	const qreal linearProgress = value;
	const qreal mix = qt_smoothBeginEndMixFactor(value);
	return  linearProgress * mix + lnprogress* (1-mix) ;
}

static qreal slow_start( qreal value ) 
{
	const qreal expprogress = exp_normal(value);
	const qreal linearProgress = value;
	const qreal mix = qt_smoothBeginEndMixFactor(value);
	return   expprogress * mix + linearProgress* (1-mix) ;
}


static qreal slow_end_pow_20(qreal value)
{
    return pow(value, 0.20);
}

static qreal slow_end_pow_22(qreal value)
{
    return pow(value, 0.22);
}

static qreal slow_end_pow_25(qreal value)
{
    return pow(value, 0.25);
}

static qreal slow_end_pow_30(qreal value)
{
    return pow(value, 0.30);
}

static qreal slow_end_pow_35(qreal value)
{
    return pow(value, 0.35);
}

static qreal slow_end_pow_40(qreal value)
{
    return pow(value, 0.40);
}

//=================================================================

QEasingCurve& easingCurve(CLAnimationCurve ac)
{
    static QEasingCurve inOutQuad(QEasingCurve::InOutQuad);
    static QEasingCurve inOutBack(QEasingCurve::InOutBack);
    static QEasingCurve outCubic(QEasingCurve::OutCubic);

    static QEasingCurve slowEnd;
    slowEnd.setCustomType(slow_end);

    static QEasingCurve slowStart;
    slowEnd.setCustomType(slow_start);


    static QEasingCurve slowEnd_pow_20;
    slowEnd_pow_20.setCustomType(slow_end_pow_20);

    static QEasingCurve slowEnd_pow_22;
    slowEnd_pow_22.setCustomType(slow_end_pow_22);


    static QEasingCurve slowEnd_pow_25;
    slowEnd_pow_25.setCustomType(slow_end_pow_25);


    static QEasingCurve slowEnd_pow_30;
    slowEnd_pow_30.setCustomType(slow_end_pow_30);

    static QEasingCurve slowEnd_pow_35;
    slowEnd_pow_35.setCustomType(slow_end_pow_35);

    static QEasingCurve slowEnd_pow_40;
    slowEnd_pow_40.setCustomType(slow_end_pow_40);

    switch(ac)
    {
    case SLOW_END:
        return slowEnd;

    case SLOW_END_POW_20:
        return slowEnd_pow_20;

    case SLOW_END_POW_22:
        return slowEnd_pow_22;

    case SLOW_END_POW_25:
        return slowEnd_pow_25;

    case SLOW_END_POW_30:
        return slowEnd_pow_30;

    case SLOW_END_POW_35:
        return slowEnd_pow_35;

    case SLOW_END_POW_40:
        return slowEnd_pow_40;

    case SLOW_START:
        return slowStart;

    case SLOW_START_SLOW_END:
        return inOutQuad;

    case INOUTBACK:
        return inOutBack;

    case OUTCUBIC:
        return outCubic;

    default:
        break;
    }

    return inOutQuad;
}
