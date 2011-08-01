#ifndef cl_long_runnable_146
#define cl_long_runnable_146

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
	  m_onPause(false)
	  {

	  }

	virtual ~CLLongRunnable(){};

    virtual void pleaseStop()
	{
		m_needStop = true;
		if (m_onPause)
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
		m_semaphore.tryAcquire(m_semaphore.available());
		m_onPause = true;

	}

	virtual void resume()
	{
		m_onPause = false;
		m_semaphore.release();
	}

	void pauseDelay()
	{
		while(m_onPause && !needToStop())
			m_semaphore.tryAcquire(1,50);
	}

        bool onPause() const { return m_onPause; }

protected:
	bool m_runing;
	volatile bool m_needStop;

	bool m_onPause;
	QSemaphore m_semaphore;
};

#endif //cl_long_runnable_146
