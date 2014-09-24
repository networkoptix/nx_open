#ifndef abstract_dts_factory_238
#define abstract_dts_factory_238

#include "core/resource/resource_fwd.h"

class QnAbstractArchiveDelegate;

class QnAbstractDTSFactory
{
public:
    QnAbstractDTSFactory(const QString& dtsId):
    m_dtsID(dtsId)
    {

    };

    virtual QnAbstractArchiveDelegate* createDeligate(const QnResourcePtr& res) = 0;
    QString getDtsID() const // normaly dts IP address or so 
    {
        return m_dtsID;
    };
private:
    QString m_dtsID;
};

struct QnDtsUnit
{
    QString resourceID;
    QnAbstractDTSFactory *factory;
};


#endif // abstract_dts_factory_238
