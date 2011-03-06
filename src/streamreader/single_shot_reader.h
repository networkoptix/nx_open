#ifndef single_shot_reader_h_105
#define single_shot_reader_h_105

#include "streamreader.h"

struct CLAbstractMediaData;

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class CLSingleShotStreamreader : public CLStreamreader
{
public:
	CLSingleShotStreamreader(CLDevice* dev );
	~CLSingleShotStreamreader(){stop();}
protected:
	virtual CLAbstractMediaData* getData() = 0;
private:
	void run(); // takes images from camera and put into queue
};

/**/

#endif //single_shot_reader_h_105