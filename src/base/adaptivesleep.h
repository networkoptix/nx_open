#ifndef cl_adaptive_sleep_137
#define cl_adaptive_sleep_137

#include "sleep.h"

#include "log.h"

const int MAX_VALID_SLEEP_TIME = 5000000;

class CLAdaptiveSleep
{
public:
	CLAdaptiveSleep(int max_overdraft ):
	  m_firstTime(true),
	  m_max_overdraft(max_overdraft)
	  {

	  };

      void setMaxOverdraft(int max_overdraft) 
      {
        m_max_overdraft = max_overdraft;
        afterdelay();
      }

      int sleep(qint64 mksec)
      {

          if (m_firstTime)
          {
              m_firstTime = false;
              m_prevEndTime.start();
              m_totalTime = 0;
          }

          m_totalTime += mksec;

          qint64 now = (qint64)m_prevEndTime.elapsed()*1000;
          qint64 havetowait = m_totalTime - now;

          if (havetowait<=0)
          {
              if (-havetowait > m_max_overdraft && m_max_overdraft > 0)
                  afterdelay();
              return havetowait;
          }
          if (havetowait < MAX_VALID_SLEEP_TIME)
            CLSleep::msleep(havetowait/1000);
          else
              afterdelay();
          return havetowait;
      }

      int addQuant(qint64 mksec)
      {
          if (m_firstTime)
          {
              m_firstTime = false;
              m_prevEndTime.start();
              m_totalTime = 0;
          }
          m_totalTime += mksec;
          qint64 now = (qint64)m_prevEndTime.elapsed()*1000;
          qint64 havetowait = m_totalTime - now;
          return havetowait;
      }


	void afterdelay()
	{
		m_firstTime = true;
	}

private:
	QTime  m_prevEndTime;
	bool m_firstTime;
	qint64 m_max_overdraft;
    qint64 m_totalTime;

};

#endif //cl_adaptive_sleep_137
