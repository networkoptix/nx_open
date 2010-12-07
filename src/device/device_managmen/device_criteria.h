#ifndef CLDeviceCriteria_h_2120
#define CLDeviceCriteria_h_2120

#include <QString>

struct CLDeviceCriteria // later
{
public:
	enum CriteriaType {ALL, NONE};
	CLDeviceCriteria(CriteriaType cr);

	CriteriaType getCriteria() const;

	void setRecorderId(QString id);
	QString getRecorderId() const;


protected:
	CriteriaType mCriteria;
	QString mRecorderId;
};


#endif //CLDeviceCriteria_h_2120