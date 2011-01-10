#include "av_singesensor.h"
#include "../streamreader/cpul_tftp_stremreader.h"
#include "../../../base/log.h"


CLArecontSingleSensorDevice::CLArecontSingleSensorDevice(int model):
CLAreconVisionDevice(model),
m_hastestPattern(true)
{

}

bool CLArecontSingleSensorDevice::getDescription()
{

	char buff[65];
	buff[64] = 0;

	for(int i = 0; i < 32; i++)
	{
		int val;

		if (getRegister(3,143+i,val)!=CL_HTTP_SUCCESS)
			return false;


		buff[2*i] = (val >> 8) & 0xFF;
		buff[2*i+1] = val & 0xFF; // 31*2+1 = 63

		if(buff[2*i] == 0 || buff[2*i+1]==0)
			break;
	}


	m_description = buff;

	return true;
}

CLStreamreader* CLArecontSingleSensorDevice::getDeviceStreamConnection()
{
	cl_log.log("Creating streamreader for ", getIP().toString(), cl_logDEBUG1);
	return new AVClientPullSSTFTPStreamreader(this);
}

bool CLArecontSingleSensorDevice::getBaseInfo()
{
	int val;

	if (m_model%10==5 && m_model!=AV3135) // if this is H.264 cam, it might be megadome
	{
		if (getRegister(3,116,val)!=CL_HTTP_SUCCESS)
			return false;

		if ((val%100)==55)//megadome
			setParam("MegaDome",1);
	}


	if (m_model%10==0 && m_model!=AV3130)  // could be compact 
	{
		if (getRegister(3,116,val)!=CL_HTTP_SUCCESS)
			return false;

		if (val == 1310 || val == 2110 ||val == 3110 || val == 5110 )//compact
		{
			
			m_hastestPattern = false;
			setParam("Compact",1);
		}

		

	}



	if (m_model == AV3135 || m_model == AV3130)
		setParam("Day/Night switcher enable", 1);


	CLValue dn;
	getParam("Day/Night switcher enable", dn);

	if (dn!="1")// this is not 3135 and not 3130; need to check if IRswitcher_present
	{
		if (getRegister(3,0x42,val)!=CL_HTTP_SUCCESS)
			return false;

		if (222==val)
		{
			setParam("Day/Night switcher enable", 1);
			dn = 1;
		}

		
	}
	
	if (dn!="1") // only if DN not present we check for AI
	{
		if (getRegister(3,0x50,val)!=CL_HTTP_SUCCESS)
			return false;

		if (val&2) // AI present
			setParam("AI present", 1);
	}
	

	return CLAreconVisionDevice::getBaseInfo();
}

QString CLArecontSingleSensorDevice::getFullName() 
{
	QString result;
	QTextStream str(&result);

	CLValue md;
	getParam("MegaDome",md);

	CLValue compact;
	getParam("Compact",compact);

	int addition = 0;
	if (md==1)
		addition += 50;

	if (compact)
		addition += 10;
	
	str << m_model+addition;




	CLValue dn;
	getParam("Day/Night switcher enable", dn);

	if (dn==1)// this is not 3135 and not 3130; need to check if IRswitcher_present
		str<<"DN";

	CLValue ai;
	getParam("AI present", ai);
	if (ai==1)// this is not 3135 and not 3130; need to check if IRswitcher_present
		str<<"AI";


	return result;
	
}

bool CLArecontSingleSensorDevice::hasTestPattern() const
{
	return m_hastestPattern;
}