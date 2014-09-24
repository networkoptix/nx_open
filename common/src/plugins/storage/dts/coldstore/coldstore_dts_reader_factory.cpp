#include "coldstore_dts_reader_factory.h"

#ifdef ENABLE_COLDSTORE

#include "coldstore_dts_archive_reader.h"

QnColdstoreDTSreaderFactory::QnColdstoreDTSreaderFactory(const QString& dtsId):
QnAbstractDTSFactory(dtsId)
{

}

QnColdstoreDTSreaderFactory::~QnColdstoreDTSreaderFactory()
{

}

QnAbstractArchiveDelegate* QnColdstoreDTSreaderFactory::createDeligate(const QnResourcePtr& res)
{
    Q_UNUSED(res)
    return new QnColdStoreDelegate(QHostAddress(getDtsID()));
}

#endif // ENABLE_COLDSTORE
