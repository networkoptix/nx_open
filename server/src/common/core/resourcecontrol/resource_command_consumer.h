#ifndef CLDeveceCommandProcessor_h_1207
#define CLDeveceCommandProcessor_h_1207

#include "datapacket/datapacket.h"
#include "dataconsumer/dataconsumer.h"
#include "resource/resource_consumer.h"


class QnResourceCommand : public QnAbstractDataPacket, public QnResourceConsumer
{
public:
	QnResourceCommand(QnResource* res);
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

	bool hasSuchDeviceInQueue(QnResource* dev) const;

protected:
	virtual void processData(QnAbstractDataPacketPtr data);
private:
	QMap<QnResource*, unsigned int> mDevicesQue;
	mutable QMutex m_cs;
};

#endif //CLDeveceCommandProcessor_h_1207
