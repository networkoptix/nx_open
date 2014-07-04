#ifndef cs_dts_reader_factory_h_558
#define cs_dts_reader_factory_h_558
#include "../abstract_dts_reader_factory.h"

class QnAbstractArchiveDelegate;

class QnColdstoreDTSreaderFactory : public QnAbstractDTSFactory
{
public:
    QnColdstoreDTSreaderFactory(const QString& dtsId);
    virtual ~QnColdstoreDTSreaderFactory();

    QnAbstractArchiveDelegate* createDeligate(const QnResourcePtr& res);
private:
    

};


#endif //cs_dts_reader_factory_h_558
