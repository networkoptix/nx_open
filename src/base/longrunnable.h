#ifndef cl_long_runnable_146
#define cl_long_runnable_146

#include <QThread>
#include <QSemaphore>

class CLLongRunnable : public QThread
{
	Q_OBJECT
public slots:
	void start ( Priority priority = InheritPriority ) 
	{
		if (m_runing) // already runing;
			return;

		m_runing = true;
		m_needStop = false;
		QThread::start(priority);
	}
public:

	CLLongRunnable():
	  m_runing(false),
	  m_onpause(false)
	  {

	  }

	virtual ~CLLongRunnable(){};

    virtual void pleaseStop()
	{
		m_needStop = true;
		if (m_onpause)
			resume();
	}

	virtual bool needToStop() const
	{
		return m_needStop;
	}

	virtual void stop()
	{
		pleaseStop();
		wait();
		m_runing = false;
	}

	virtual void pause()
	{
		m_sem.tryAcquire(m_sem.available());
		m_onpause = true;
		
	}

	virtual void resume()
	{
		m_onpause = false;
		m_sem.release();
	}

	void pause_delay()
	{
		while(m_onpause && !needToStop())
			m_sem.tryAcquire(1,50);
	}

protected:
	bool m_runing;
	volatile bool m_needStop;

	bool m_onpause;
	QSemaphore m_sem;
};

#endif //cl_long_runnable_146