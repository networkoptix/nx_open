#ifndef statistics_reader_h_1
#define statistics_reader_h_1

#include "core/dataprovider/media_streamdataprovider.h"
#include "api/video_server_connection.h"

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class QnStatisticsReader : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
	QnStatisticsReader(QnResourcePtr resource);
	~QnStatisticsReader(){stop();}
    void setApiConnection(QnVideoServerConnectionPtr connection);
public slots:
    void at_statistics_received(int usage);
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void run() override;
private:
    void drawStatistics(int width, int height, QPainter *painter);

    QnVideoServerConnectionPtr m_api_connection;
    QList<int> m_history;
    QList<int> m_steps;
};

/**/

#endif //statistics_reader_h_1
