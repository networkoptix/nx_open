#ifndef statistics_reader_h_1
#define statistics_reader_h_1

#include "core/dataprovider/media_streamdataprovider.h"

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class QnStatisticsReader : public QnAbstractMediaStreamDataProvider
{
public:
	QnStatisticsReader(QnResourcePtr resource);
	~QnStatisticsReader(){stop();}
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void run() override;
};

/**/

#endif //statistics_reader_h_1
