#ifndef coldstore_dts_searcher_h_1807
#define coldstore_dts_searcher_h_1807

#ifdef ENABLE_COLDSTORE

#include "../abstract_dts_reader_factory.h"
#include "../abstract_dts_searcher.h"

#include <QtNetwork/QHostAddress>
#include <QtCore/QMap>


class QnColdStoreDTSSearcher: public QnAbstractDTSSearcher
{
	QnColdStoreDTSSearcher();
public:

	~QnColdStoreDTSSearcher();

	static QnColdStoreDTSSearcher& instance();

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

#endif // ENABLE_COLDSTORE

#endif // coldstore_dts_searcher_h_1807
