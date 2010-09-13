#include "animated_bgr.h"

#define MAX_STEP 600
#include <QColor>
#include <QRadialGradient>
#include <QPainter>

CLAnimatedBackGround::CLAnimatedBackGround(int timestep):
mTimeStep(timestep),
mForward(true),
mSteps(0)
{
	mTime.restart();
}

bool CLAnimatedBackGround::tick()
{
	QTime now = QTime::currentTime();
	if (mTime.msecsTo(now)>mTimeStep)
	{
		mTime = now;
		return true;
	}

	return false;
}

int CLAnimatedBackGround::currPos()
{
	if (tick())
	{
		if (mForward)
		{
			++mSteps;

			if (mSteps==MAX_STEP)
				mForward = false;
		}
		else
		{
			--mSteps;
			if (mSteps==-MAX_STEP)
				mForward = true;
		}
	}

	return mSteps;
}

qreal CLAnimatedBackGround::currPosRelative()
{
	return qreal(currPos())/MAX_STEP;
}


CLBlueBackGround::CLBlueBackGround(int timestep):
CLAnimatedBackGround(timestep)
{

}

void CLBlueBackGround::drawBackground(QPainter * painter, const QRectF & rect )
{
	qreal dx_unit = rect.width()/MAX_STEP;
	qreal dy_unit = rect.height()/MAX_STEP;
	qreal rpos = currPosRelative();
	
	if (rpos<0)
		rpos = rpos;


	QColor bl(10,10,140+50*rpos,255);
	//QColor bl(10,100+50*rpos,10,255);


	QPointF center1( rect.center().x() - rpos*rect.width()/2, rect.center().y() );
	QPointF center2( rect.center().x() + rpos*rect.width()/2, rect.center().y() );

	
	{
		
		QRadialGradient radialGrad(center1, qMin(rect.width(), rect.height())/1.5);
		radialGrad.setColorAt(0, bl);
		radialGrad.setColorAt(1, QColor(0,0,0,0));
		painter->fillRect(rect, radialGrad);
	}

	{
		
		QRadialGradient radialGrad(center2, qMin(rect.width(), rect.height())/1.5);
		radialGrad.setColorAt(0, bl);
		radialGrad.setColorAt(1, QColor(0,0,0,0));
		painter->fillRect(rect, radialGrad);
	}

	/**/

}
