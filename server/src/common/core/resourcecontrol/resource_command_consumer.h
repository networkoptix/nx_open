#ifndef CLDeveceCommandProcessor_h_1207
#define CLDeveceCommandProcessor_h_1207

#include "datapacket/datapacket.h"
#include "dataconsumer/dataconsumer.h"
#include "resource/resource_consumer.h"
#include "resource/id.h"


class QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
	QnResourceCommand(QnResourcePtr res);
	virtual ~QnResourceCommand();
	virtual void execute() = 0;

	

};

typedef QSharedPointer<QnResourceCommand> QnResourceCommandPtr;


class QnResourceCommandProcessor : public QnAbstractDataConsumer
{
public:
	QnResourceCommandProcessor();
	~QnResourceCommandProcessor();

	virtual void putData(QnAbstractDataPacketPtr data);

	virtual void clearUnprocessedData();

	bool hasSuchResourceInQueue(QnResourcePtr res) const;

protected:
	virtual void processData(QnAbstractDataPacketPtr data);
private:
    mutable QMutex m_cs;
	QMap<QnId, unsigned int> mResourceQueue;
	
};

#endif //CLDeveceCommandProcessor_h_1207
