#ifndef resource_commendprocessor_h_2221
#define resource_commendprocessor_h_2221

#include "streamreader/streamreader.h"
#include "data/dataprocessor.h"

class QN_EXPORT QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
    QnResourceCommand(QnResource* res);
    virtual ~QnResourceCommand();
    virtual void execute() = 0;
};

typedef QSharedPointer<QnResourceCommand> QnResourceCommandPtr;

class QN_EXPORT QnResourceCommandProcessor : public QnAbstractDataConsumer
{
public:
	QnResourceCommandProcessor();
	~QnResourceCommandProcessor();

	virtual void putData(QnAbstractDataPacketPtr data);

	virtual void clearUnprocessedData();

	bool hasSuchResourceInQueue(QnResourcePtr res) const;

protected:
	virtual bool processData(QnAbstractDataPacketPtr data);
private:
    mutable QMutex m_cs;
	QMap<QnId, unsigned int> mResourceQueue;
	
};

#endif //resource_commendprocessor_h_2221
