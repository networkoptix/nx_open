#include "abstract_dts_reader_factory.h"

class QnAbstractDTSSearcher
{
public:
    virtual QList<QnDtsUnit> findDtsUnits() = 0;
};
