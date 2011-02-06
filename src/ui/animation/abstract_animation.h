#ifndef abstract_animation_h_2000
#define abstract_animation_h_2000

class QObject;

class CLAbstractAnimation 
{

public:
	CLAbstractAnimation();
	virtual ~CLAbstractAnimation();

	virtual void stopAnimation()=0;
	virtual bool isRuning() const = 0;
	void setDeleteAfterFinished(bool val);
	bool deleteAfterFinished() const;

	virtual QObject* object()  = 0;

protected:
	bool m_deleteAfterFinished;

};



#endif //abstract_animation_h_2000