#ifndef CLDeveceCommandProcessor_h_1207
#define CLDeveceCommandProcessor_h_1207

#include <QMap>
#include <QMutex>
#include <QSharedPointer>

#include "data/dataprocessor.h"
#include "data/data.h"

class QnResource;

class QN_EXPORT QnDeviceCommand : public QnAbstractDataPacket
{
public:
	QnDeviceCommand(QSharedPointer<QnResource> device);
	virtual ~QnDeviceCommand();
	QSharedPointer<QnResource> getDevice() const;
	virtual void execute() = 0;
protected:
	QSharedPointer<QnResource> m_device;

};
typedef QSharedPointer<QnDeviceCommand> QnDeviceCommandPtr;

class QN_EXPORT CLDeviceCommandProcessor : public QnAbstractDataConsumer
{
public:
	CLDeviceCommandProcessor();
	~CLDeviceCommandProcessor();

	virtual void putData(QnAbstractDataPacketPtr data);

	virtual void clearUnprocessedData();

	bool hasSuchDeviceInQueue(const QString& dev) const;

protected:
	virtual bool processData(QnAbstractDataPacketPtr data);
private:
	QMap<QSharedPointer<QnResource>, unsigned int> mDevicesQue;
	mutable QMutex m_cs;
};

#endif //CLDeveceCommandProcessor_h_1207
