#ifndef av_device_server_h_2107
#define av_device_server_h_2107

#include "core/resourcemanagment/resourceserver.h"

class QnPlArecontResourceSearcher : public QnAbstractNetworkResourceSearcher
{
	QnPlArecontResourceSearcher();
public:

	~QnPlArecontResourceSearcher(){};

	static QnPlArecontResourceSearcher& instance();
	
    bool isResourceTypeSupported(const QnId& resourceTypeId) const;

    QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
	// return the manufacture of the server 
	virtual QString manufacture() const;

	// returns all available devices 
	virtual QnResourceList findResources();

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
};

#endif // av_device_server_h_2107
