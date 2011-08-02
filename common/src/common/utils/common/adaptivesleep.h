#ifndef cl_adaptive_sleep_137
#define cl_adaptive_sleep_137

#include "sleep.h"


class CLAdaptiveSleep
{
public:
	CLAdaptiveSleep(int max_overdraft ):
	  m_firstTime(true),
	  m_max_overdraft(max_overdraft)
	  {

	  };

	void sleep(qint64 mksec)
	{

		if (m_firstTime)
		{
			m_firstTime = false;
			m_prevEndTime.start();
			m_mod = 0;
		}

		m_mod += mksec%1000;
		if (m_mod >= 1000)
		{
			mksec+=1000;
			m_mod-=1000;
		}

		qint64 past = (qint64)m_prevEndTime.elapsed()*1000;
		//cl_log.log("past = ", (int)past,  cl_logALWAYS);
		//past*=1000;

		qint64 havetowait = mksec - past;
		m_prevEndTime = m_prevEndTime.addMSecs(mksec/1000);

		if (havetowait<=0)
		{
			if (-havetowait > m_max_overdraft)
				afterdelay();

			return;
		}

		QnSleep::msleep(havetowait/1000);

	}

	void afterdelay()
	{
		m_firstTime = true;
	}

private:
	QTime  m_prevEndTime;
	bool m_firstTime;
	qint64 m_max_overdraft;

	qint64 m_mod;

};

#endif //cl_adaptive_sleep_137
