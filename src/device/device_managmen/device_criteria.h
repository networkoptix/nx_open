#ifndef CLDeviceCriteria_h_2120
#define CLDeviceCriteria_h_2120

struct CLDeviceCriteria // later
{
public:
	enum CriteriaType {ALL, NONE, FILTER};
	CLDeviceCriteria(CriteriaType cr);

	CriteriaType getCriteria() const;
	void setCriteria(CriteriaType );

	void setRecorderId(QString id);
	QString getRecorderId() const;

	void setFilter(const QString& filter); 
	QString filter() const;

protected:
	CriteriaType mCriteria;
	QString mRecorderId;
	QString mFilter;
};

#endif //CLDeviceCriteria_h_2120