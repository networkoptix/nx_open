#ifndef cpull_file_tester_1124
#define cpull_file_tester_1124


#include "../../../streamreader/cpull_stremreader.h"


class AVTestFileStreamreader : public CLClientPullStreamreader
{
public:
	explicit AVTestFileStreamreader(CLDevice* dev);
	

	~AVTestFileStreamreader();

protected:
	virtual CLAbstractMediaData* getNextData();
	
	
protected:

private:

	unsigned char* data;
	unsigned char* descr;
	unsigned char* pdata;
	int* pdescr;

	int data_len;
	int descr_data_len;



};

#endif //cpull_file_tester_1124