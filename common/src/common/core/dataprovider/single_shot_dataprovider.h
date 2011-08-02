#ifndef single_shot_reader_h_105
#define single_shot_reader_h_105


#include "media_streamdataprovider.h"

// difference between this class and pull reader is that run function does not have infinite loop
// it quits after first getData
// such reader can be used for photo 

class QnSingleShotStreamreader : public QnAbstractMediaStreamDataProvider
{
public:
	QnSingleShotStreamreader(QnResourcePtr res);
	~QnSingleShotStreamreader(){stop();}
protected:

private:
	void run(); // takes images from camera and put into queue

    virtual bool beforeGetData(){return true;};

    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data){return true;};

    virtual void beforeRun(){};
    virtual void afterRun() {};

    virtual void sleepIfNeeded(){};

};

/**/

#endif //single_shot_reader_h_105
