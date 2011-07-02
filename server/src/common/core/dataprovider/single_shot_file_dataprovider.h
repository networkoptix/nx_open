#ifndef single_shot_file_reader_h_215
#define single_shot_file_reader_h_215

#include "single_shot_dataprovider.h"

struct CLAbstractMediaData;

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class CLSingleShotFileStreamreader : public CLSingleShotStreamreader
{
public:
	CLSingleShotFileStreamreader(CLDevice* dev );
	~CLSingleShotFileStreamreader(){stop();}

protected:
	virtual CLAbstractMediaData* getData();
private:

	QString m_fileName;

};

/**/

#endif //single_shot_file_reader_h_215
