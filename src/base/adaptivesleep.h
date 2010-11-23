#ifndef cl_adaptive_sleep_137
#define cl_adaptive_sleep_137

#include "sleep.h"
#include <QTime>

#include "log.h"

class CLAdaptiveSleep
{
public:
	CLAdaptiveSleep(int max_overdraft ):
	  m_firstTime(true),
	  m_max_overdraft(max_overdraft)
	  {

	  };

	void sleep(unsigned long msecs)
	{
		if (m_firstTime)
		{
			m_firstTime = false;
			m_prevEndTime.start();
		}

		
		int past = m_prevEndTime.elapsed();


		qint32 havetowait = msecs - past;
		m_prevEndTime = m_prevEndTime.addMSecs(msecs);

		if (havetowait<=0)
		{
			if (-havetowait > m_max_overdraft)
				afterdelay();

			return;
		}

		CLSleep::msleep(havetowait);

	}

	void afterdelay()
	{
		m_firstTime = true;
	}

private:
	QTime  m_prevEndTime;
	bool m_firstTime;
	int m_max_overdraft;



};

#endif //cl_adaptive_sleep_137