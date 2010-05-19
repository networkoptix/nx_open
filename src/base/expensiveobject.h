#ifndef cl_expensive_object1338_h
#define cl_expensive_object1338_h

 #include <QAtomicInt>


class CLRefCounter
{
public:
	CLRefCounter():
		ref_counter(1)
	{

	}

	virtual ~CLRefCounter()
	{

	};

	void addRef()
	{
		ref_counter.ref();
	}

	void releaseRef()
	{
		if (!ref_counter.deref())
			delete this;

	}
private:
	QAtomicInt ref_counter;
	//void operator delete(void*); 
};


#endif //cl_ThreadQueue_h_2236