#ifndef cl_long_runnable_146
#define cl_long_runnable_146

#include <QThread>

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
	  m_runing(false)
	  {

	  }

    virtual void pleaseStop()
	{
		m_needStop = true;
	}

	virtual bool needToStop() const
	{
		return m_needStop;
	}

	virtual void stop()
	{
		m_needStop = true;
		wait();
		m_runing = false;
	}

protected:
	bool m_runing;
	volatile bool m_needStop;
};

#endif //cl_long_runnable_146