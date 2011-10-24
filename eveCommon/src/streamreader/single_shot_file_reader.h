#ifndef single_shot_file_reader_h_215
#define single_shot_file_reader_h_215

#include "single_shot_reader.h"
#include "data/mediadata.h"

struct QnAbstractMediaData;

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class CLSingleShotFileStreamreader : public CLSingleShotStreamreader
{
public:
	CLSingleShotFileStreamreader(QnResource* dev );
	~CLSingleShotFileStreamreader(){stop();}

protected:
	virtual QnAbstractMediaDataPtr getData();
private:

	QString m_fileName;

};

/**/

#endif //single_shot_file_reader_h_215
