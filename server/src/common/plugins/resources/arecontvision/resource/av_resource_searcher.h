#ifndef av_device_server_h_2107
#define av_device_server_h_2107


#include "resourcecontrol/abstract_resource_searcher.h"



class QnPlArecontResourceSearcher : public QnAbstractNetworkResourceSearcher
{
	QnPlArecontResourceSearcher();
public:

	~QnPlArecontResourceSearcher(){};

	static QnPlArecontResourceSearcher& instance();

	
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
	virtual QnResourceList findDevices();

    virtual QnResource* checkHostAddr(QHostAddress addr);

};

#endif // av_device_server_h_2107
