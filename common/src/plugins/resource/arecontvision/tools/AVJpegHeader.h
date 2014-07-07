#ifndef AVJpeg_h_442
#define AVJpeg_h_442

namespace AVJpeg
{
class Header
{
public:
	Header(void) {}
	~Header(void) {}

	static void Initialize(const char* szCompany, const char* szApplication, const char* szCopyright);
	static int GetHeader(unsigned char* pBuffer, unsigned int nWidth, unsigned int nHeight, int iQuality, const char* szCameraModel); 
	static int GetHeaderSize();

protected:
	static unsigned char s_JpegHeaderTemplate[];
	static unsigned char s_LuminTbl[25][64];
	static unsigned char s_ChromTbl[25][64];
};

const double QUALITY_TABLE[] = 
{
	3.250, 3.125, 3.000, 2.875,	2.750,
	2.625, 2.500, 2.375, 2.250,	2.125,
	2.000, 1.875, 1.750, 1.625,	1.500,
	1.375, 1.250, 1.125, 1.000, 0.825,
	0.750, 0.625, 0.500, 0.375,	0.250
};

const int JPEG_NATURAL_ORDER_TBL[] = 
{
	0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63,
};

const int STD_CHROM_TBL[] = 
//int AVJpegHeader::s_piStdQTtables[128] = 
{
	// chrominance quantization table
	8,8,12,22,48,48,48,48,
	8,10,12,32,48,48,48,48,
	12,12,28,48,48,48,48,48,
	22,32,48,48,48,48,48,48,
	48,48,48,48,48,48,48,48,
	48,48,48,48,48,48,48,48,
	48,48,48,48,48,48,48,48,
	48,48,48,48,48,48,48,52
};

const int STD_LUMIN_TBL[] = 
{
	// luminance quantization table
	8,4,4,8,12,20,24,30,
	6,6,6,8,12,28,30,26,
	6,6,8,12,20,28,34,28,
	6,8,10,14,24,42,40,30,
	8,10,18,28,34,54,50,38,
	12,16,26,32,40,52,56,46,
	24,32,38,42,50,60,60,50,
	36,46,46,48,56,50,50,52
};

const unsigned int CAMERA_MODEL_OFFSET		= 0xD0;
const unsigned int COMPANY_NAME_OFFSET		= 0x90;
const unsigned int APP_NAME_OFFSET			= 0xF0;
const unsigned int COPYRIGHT_NAME_OFFSET	= 0x130;
const unsigned int LUMIN_TBL_OFFSET			= 0x205;
const unsigned int CHROM_TBL_OFFSET			= 0x246;
const unsigned int HEIGHT_VAL_OFFSET		= 0x291;
const unsigned int WIDTH_VAL_OFFSET			= 0x293;

const unsigned int HEADER_SIZE				= 1105;

};

#endif //AVJpeg_h_442
