#include "AVJpegHeader.h"
#include <string>
#include <cstring>

#undef HIBYTE
#undef LOBYTE
#define HIBYTE(w) ((unsigned char)((((unsigned long)(w)) >> 8) & 0xff))
#define LOBYTE(w) ((unsigned char)(((unsigned long)(w)) & 0xff))

#pragma region JPEG Header template
unsigned char AVJpeg::Header::s_JpegHeaderTemplate[] = 
{
	//EXIF_Header
	0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,   //1  APP0,C010H,
	0x49, 0x46, 0x00, 0x01, 0x02, 0x01, 0x00, 0xB4,
	0x00, 0xB4, 0x00, 0x00, 0xFF, 0xE1, 0x01, 0xEA,   //   APP1,C1EAH,
	0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x49, 0x49,   //   Exif  II
	0x2A, 0x00, 0x08, 0x00, 0x00, 0x00, 0x11, 0x00,

	0x0F, 0x01, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00,   //6  Make,10FH,ASCII,C64bytes,Pos072H,Actual_090H
	0x72, 0x00, 0x00, 0x00, 0x10, 0x01, 0x02, 0x00,   //   Model,110H,ASCII,C32bytes,Pos0B2H,Actual_0D0H
	0x20, 0x00, 0x00, 0x00, 0xB2, 0x00, 0x00, 0x00,

	0x31, 0x01, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00,   //   Software,131H,ASCII,C64bytes,Pos0D2H,Actual_0F0H
	0xD2, 0x00, 0x00, 0x00, 0x98, 0x82, 0x02, 0x00,   //   CopyRight,8298H,ASCII,C64bytes,Pos112H,Actual_130H
	0x40, 0x00, 0x00, 0x00, 0x12, 0x01, 0x00, 0x00,   //11

	0x03, 0x90, 0x02, 0x00, 0x14, 0x00, 0x00, 0x00,   //   TimeOriginal,9003H,ASCII,C20bytes,Pos152H,Actual_170H
	0x52, 0x01, 0x00, 0x00, 0x04, 0x90, 0x02, 0x00,   //   TimeDigital,9004H,ASCII,C20bytes,Pos166H,Actual_184H
	0x14, 0x00, 0x00, 0x00, 0x66, 0x01, 0x00, 0x00,

	0x0E, 0x01, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00,   //   Descriptoin,10EH,ASCII,C64bytes,Pos182H,Actual_1A0H
	0x82, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //16
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //18  Make starts here, 90H
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //21
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //25  Make ends here

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //26  Model starts here, D0H
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //29  Model ends here

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //30  Software starts here, F0H
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //31
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //36
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //37  Software ends here

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //37  CopyRight starts here, 130H
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //41
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //45  CopyRight ends here

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //46  TimeOriginal starts here
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //50  TimeOriginal ends here

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //51
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //52  Descriptoin starts here, 1A0H
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //56
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //61
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    //64   Descriptoin ends here
	//EXIF_Header end

	0xff, 0xdb, 0x00, 0x84, 0x00,						//output quantization tables marker
	//luminance table placeholder - 64 bytes
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x01,												//beginning of chrominance table with 8-bit precision
	//chrominance table - 64 bytes
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xff, 0xdd,											//output DRI marker
	0x00, 0x04,											//output DRI segment length in bytes
	0x00, 0x10,											//restart interval, only one intra image encoded. Always 16
	0xff, 0xc0,											//output frame header marker
	0x00, 0x11,											//output frame header length
	0x08,												//sample precision
	0x00,												//Image rows / 256														offset 657
	0x00,												//Image rows % 256
	0x00,												//Image cols / 256
	0x00,												//Image cols % 256
	0x03,												//number of components in frame
	0x01, 0x21, 0x00,									//first component description - luminance								offset 662
	0x02, 0x11, 0x01,									//second component description											offset 665
	0x03, 0x11, 0x01,									//third component description											offset 668
	0xff, 0xc4,											//output huffman table marker
	0x01, 0xa2,											//length of DHT segment
	0x00,												//luminance dc table descriptor (class 0; identifier 0)					offset 672
	//number of huffman codes - 16 bytes
	0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		//																		offset 681
	//associated values
	0x00, 0x01, 0x02, 0x03,								//																		offset 689
	0x04, 0x05, 0x06, 0x07,								//																		offset 693
	0x08, 0x09, 0x0A, 0x0B,								//																		offset 697

	0x10,												//luminance ac table descriptor (class 1; identifier 0)					offset 701

	//number of huffman codes - 16 bytes
	0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,		//																		offset 702 
	0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d,		//																		offset 710

	//associated values - 162 bytes
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,	//																	offset 718
	0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 
	0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 
	0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 
	0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 
	0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 
	0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 
	0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 
	0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 
	0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 
	0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 
	0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,

	0x01, //chrominance dc table descriptor (class 0; identifier 1)																offset 880

	//number of huffman codes
	0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,		//																		offset 881 
	0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,		//																		offset 889 
	//associated values
	0x00, 0x01, 0x02, 0x03,								//																		offset 897 					
	0x04, 0x05, 0x06, 0x07,								//																		offset 901 
	0x08, 0x09, 0x0A, 0x0B,								//																		offset 905 

	0x11,	//chrominance ac table descriptor (class 1; identifier 1)															offset 909 

	//number of huffman codes
	0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,		//																		offset 910 
	0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,

	//associated values - 162 bytes
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,	0x31, 
	0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 
	0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 
	0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 
	0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 
	0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 
	0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 
	0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 
	0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 
	0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 
	0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 
	0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 
	0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 
	0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,

	0xff, 0xda, //output SOS  marker
	0x00, 0x0c, //length of SOS header segment
	0x03, //number of components in scan
	0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00	//descriptors of components in scan
};
#pragma endregion

unsigned char AVJpeg::Header::s_LuminTbl[25][64];
unsigned char AVJpeg::Header::s_ChromTbl[25][64];

//Static
int AVJpeg::Header::GetHeader(unsigned char* pBuffer, unsigned int nWidth, unsigned int nHeight, int iQuality, const char* szCameraModel)
{
#ifndef _countof
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

	memcpy(pBuffer, s_JpegHeaderTemplate, _countof(s_JpegHeaderTemplate));

	if(szCameraModel && *szCameraModel)
	{
		strncpy((char*)(pBuffer + CAMERA_MODEL_OFFSET), szCameraModel, 32);
	}

	//output luminance and chrominance tables
	memcpy(pBuffer + LUMIN_TBL_OFFSET, s_LuminTbl[iQuality - 1], _countof(s_LuminTbl[iQuality - 1]));
	memcpy(pBuffer + CHROM_TBL_OFFSET, s_ChromTbl[iQuality - 1], _countof(s_ChromTbl[iQuality - 1]));

	//Image height
	pBuffer[HEIGHT_VAL_OFFSET] = HIBYTE(nHeight);
	pBuffer[HEIGHT_VAL_OFFSET + 1] = LOBYTE(nHeight);
	//Image width 
	pBuffer[WIDTH_VAL_OFFSET] = HIBYTE(nWidth);
	pBuffer[WIDTH_VAL_OFFSET + 1] = LOBYTE(nWidth);

	return _countof(s_JpegHeaderTemplate);
}
//Static
void AVJpeg::Header::Initialize(const char* szCompany, const char* szApplication, const char* szCopyright)
{
	if(szCompany && *szCompany)
	{
		strncpy((char*)(s_JpegHeaderTemplate + COMPANY_NAME_OFFSET), szCompany, 64);
	}
	if(szApplication && *szApplication)
	{
		strncpy((char*)(s_JpegHeaderTemplate + APP_NAME_OFFSET), szApplication, 64);
	}
	if(szCopyright && *szCopyright)
	{
		strncpy((char*)(s_JpegHeaderTemplate + COPYRIGHT_NAME_OFFSET), szCopyright, 64);
	}

	//Prepare Luminance and Chrominance tables for dfferent qualities index
	for(int iQuality = 0; iQuality < 25; iQuality++)
	{
		for(int i = 0; i < 64; i++)
		{
			s_LuminTbl[iQuality][i] = (unsigned char)(STD_LUMIN_TBL[JPEG_NATURAL_ORDER_TBL[i]] * QUALITY_TABLE[iQuality] + 0.5);
			s_ChromTbl[iQuality][i] = (unsigned char)(STD_CHROM_TBL[JPEG_NATURAL_ORDER_TBL[i]] * QUALITY_TABLE[iQuality] + 0.5);
		}
	}
}

//Static
int AVJpeg::Header::GetHeaderSize()
{
	return sizeof(s_JpegHeaderTemplate);
}

