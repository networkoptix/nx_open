#include "device_criteria.h"


//=============================================================
CLDeviceCriteria::CLDeviceCriteria(CriteriaType cr):
mCriteria(cr)
{

}

CLDeviceCriteria::CriteriaType CLDeviceCriteria::getCriteria() const
{
	return mCriteria;
}

void CLDeviceCriteria::setRecorderId(QString id)
{
	mRecorderId = id;
}

QString CLDeviceCriteria::getRecorderId() const
{
	return mRecorderId;
}

