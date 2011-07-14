#ifndef single_shot_reader_h_105
#define single_shot_reader_h_105


#include "streamdataprovider.h"
#include "datapacket/mediadatapacket.h"


// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class CLSingleShotStreamreader : public QnMediaStreamDataProvider
{
public:
	CLSingleShotStreamreader(QnResource* dev );
	~CLSingleShotStreamreader(){stop();}
protected:
	virtual QnAbstractMediaDataPacketPtr getData() = 0;
private:
	void run(); // takes images from camera and put into queue
};

/**/

#endif //single_shot_reader_h_105
