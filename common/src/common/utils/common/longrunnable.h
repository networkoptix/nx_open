#ifndef cl_long_runnable_146
#define cl_long_runnable_146

#include <QtCore/QSemaphore>
#include <QtCore/QThread>

class QnLongRunnable : public QThread
{
	Q_OBJECT

public Q_SLOTS:
	void start ( Priority priority = InheritPriority )
	{
		if (m_runing) // already runing;
			return;

		m_runing = true;
		m_needStop = false;
		QThread::start(priority);
	}

public:
	QnLongRunnable():
	  m_runing(false),
	  m_onPause(false)
	  {
	  }

	virtual ~QnLongRunnable(){};

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

    virtual bool isPaused() const { return m_onPause; }

	virtual void resume()
	{
		m_onPause = false;
		m_semaphore.release();
	}

	void pauseIfNeeded()
	{
		while(m_onPause && !needToStop())
			m_semaphore.tryAcquire(1,50);
	}

protected:
	bool m_runing;
	volatile bool m_needStop;

	bool m_onPause;
	QSemaphore m_semaphore;
};

#endif //cl_long_runnable_146
