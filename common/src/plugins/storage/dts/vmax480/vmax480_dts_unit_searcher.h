#ifndef vmax480_dts_searcher_h_1759
#define vmax480_dts_searcher_h_1759

#include "../abstract_dts_reader_factory.h"
#include "../abstract_dts_searcher.h"

class QnWMax480DTSSearcher: public QnAbstractDTSSearcher
{
	QnWMax480DTSSearcher();
public:

	~QnWMax480DTSSearcher();

	static QnWMax480DTSSearcher& instance();

	// returns all available devices 
    virtual QList<QnDtsUnit> findDtsUnits() override;

protected:
    
    QnAbstractDTSFactory* getFactoryById();


private:
    void requestFileList(QList<QnDtsUnit>& result, QHostAddress addr);

private:

    QMap<QString, QnAbstractDTSFactory*> m_factoryList;

    QByteArray *m_request;

};

#endif // vmax480_dts_searcher_h_1759
