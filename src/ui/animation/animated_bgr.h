#ifndef animated_bgr_h_1600
#define animated_bgr_h_1600

class QPainter;

class CLAnimatedBackGround
{
public:
	CLAnimatedBackGround(int timestep);
	virtual void drawBackground ( QPainter * painter, const QRectF & rect )=0;
protected:
	bool tick();
	int currPos();
	qreal currPosRelative();
protected:
	QTime mTime;
	bool mForward;
	int mSteps;
	int mTimeStep;

};

class CLBlueBackGround : public CLAnimatedBackGround
{
public:
	CLBlueBackGround(int timestep);
	virtual void drawBackground ( QPainter * painter, const QRectF & rect );
};

#endif //animated_bgr_h_1600