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

void CLDeviceCriteria::setCriteria(CriteriaType cr)
{
	this->mCriteria = cr;
}

void CLDeviceCriteria::setFilter(const QString& filter)
{
	mFilter = filter;
}

QString CLDeviceCriteria::filter() const
{
	return mFilter;
}