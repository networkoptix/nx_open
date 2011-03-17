#ifndef mouse_state_h1958
#define mouse_state_h1958

class QMouseEvent;

class CLMouseState
{
public:
	void mouseMoveEventHandler(QMouseEvent *event);
	void clearEvents();

	QPoint getLastEventPoint() const;

	// pay attantion to the fact that method is not const! it changes m_mouseTrackQueue
	void getMouseSpeed(qreal& speed, qreal& h_speed, qreal& v_speed);
private:

private:
	QQueue<QPair<QPoint, QTime> > m_mouseTrackQueue;

};

#endif //mouse_state_h1958

