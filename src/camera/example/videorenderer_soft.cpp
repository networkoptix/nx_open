
#include <QQueue>
#include <QRect>

#include "yuvconvert.h"
#include "videorenderer_soft.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QApplication>
#include <QtCore/QTime>
#include <QMutex>
#include <QDebug>
#include <QVariant>
#include <QtGui/QDesktopWidget>
#include <cmath> //for sin and cos
#include <QMessageBox>


#if defined(Q_OS_WIN32)
#include <Qt/private/qwindowsurface_p.h>
#elif defined(Q_OS_MAC)
#import <Carbon/Carbon.h>
#else
#include "qt-4.3.3/qwindowsurface_p.h"
// #include "qt-4.5.0/qwindowsurface_p.h"
#endif

#include "ffmpeg/qtvvideodataupdater.h"
#include "ffmpeg/qtvvideodatareader.h"

//#define ADDITION_TEST_VIDEO_DRIVER

//this will make a display every second of how many frames were pocessed and actually displayed

#include <QtOpenGL/QGLWidget>
#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB           0x8804
#define GL_PROGRAM_FORMAT_ASCII_ARB       0x8875
#endif

// support old OpenGL installations (1.2)
// assume that if TEXTURE0 isn't defined, none are
#ifndef GL_TEXTURE0
# define GL_TEXTURE0    0x84C0
# define GL_TEXTURE1    0x84C1
# define GL_TEXTURE2    0x84C2
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE	0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_SGIS
#define GL_CLAMP_TO_EDGE_SGIS 0x812F
#endif

#ifndef GL_CLAMP_TO_EDGE_EXT
#define GL_CLAMP_TO_EDGE_EXT 0x812F
#endif



// arbfp1 fragment program for converting yuv (YV12) to rgb
static const char yv12ToRgb[] =
"!!ARBfp1.0"
"PARAM c[6] = { program.local[0..1],"
"                { 1.164, 0, 1.596, 0.5 },"
"                { 0.0625, 1.164, -0.391, -0.813 },"
"                { 1.164, 2.018, 0 },"
"				{ 1.164, -0.391, -0.81300002, -0.813 } };"
"TEMP R0;"
"TEX R0.x, fragment.texcoord[0], texture[1], 2D;"
"ADD R0.y, R0.x, -c[2].w;"
"TEX R0.x, fragment.texcoord[0], texture[2], 2D;"
"ADD R0.x, R0, -c[2].w;"
"MUL R0.z, R0.y, c[0].w;"
"MAD R0.z, R0.x, c[0], R0;"
"MUL R0.w, R0.x, c[0];"
"MUL R0.z, R0, c[0].y;"
"TEX R0.x, fragment.texcoord[0], texture[0], 2D;"
"MAD R0.y, R0, c[0].z, R0.w;"
"ADD R0.x, R0, -c[3];"
"MUL R0.y, R0, c[0];"
"MUL R0.z, R0, c[1].x;"
"MAD R0.x, R0, c[0].y, c[0];"
"MUL R0.y, R0, c[1].x;"
"DP3 result.color.x, R0, c[2];"
"DP3 result.color.y, R0, c[5];"
"DP3 result.color.z, R0, c[4];"
"MOV result.color.w, c[1].y;"
"END";

class VideoRendererSoftFilter : public QtvVideoDataUpdater
{
public:
    VideoRendererSoftFilter(VideoRendererSoft *renderer, QWidget* widget);

    ~VideoRendererSoftFilter();

    void freeGLResources()
    {
		m_videoTextureReady = false;
		m_updateFullAppLayer = true;
    }

    void freeResources()
    {
		m_stopped = true;
        QMutexLocker locker(&m_mutex);
        freeGLResources();
        m_textureUploaded = false;
    }

    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
    {
        //let's normalize the values
        m_brightness = brightness * 128;
        m_contrast = contrast + 1.;
        m_hue = hue * 180.;
        m_saturation = saturation + 1.;
    }
    //the following function is called from the GUI thread
	void drawTexture(GLuint texture, float wCoeff, float hCoeff, const float* tvArray = NULL) const;
	void drawAppLayer(const QRect& r) const;
    void repaintCurrentFrame(QPainter &painter, const QRect &r);
	void getTextureRect(QRect& drawRect, float textureWidth, float textureHeight,
		float windowWidth, float windowHeight, const float sar) const;
	int checkOpenGLError() const;
#ifdef _DEBUG
	int checkOpenGLErrorDebug() const;
#endif
	void setAspectRatio(uint ar);
	void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
	void setTextureParameters() const;
	void Clear() const;
	void SwapBuffers() const;
	int getMinPow2(int value) const
	{
		int rez = 1;
		for (; value > rez; rez <<= 1);
		return rez;
	}
	QSize videoSize() const;
	void initSoftYUV2RBG();
	const uchar* screenShoot(const char* fileName = NULL) const;
	virtual bool update(const uchar** pixels, uint width, uint height, int* stride);
	virtual void reset();
private:
	int appTextureWidth;
	int appTextureHeight;

    VideoRendererSoft* m_renderer;
    mutable QMutex m_mutex;
	const uchar** arrayPixels;
    int m_width, m_height;
    bool m_textureUploaded;

    //mixer settings
    qreal m_brightness,
          m_contrast,
          m_hue,
          m_saturation;
	// ffmpeg variables
	QRect videoRect;
	qreal painterOpacity;
	QMutex* textureMutex;
	QtvVideoDataReader* qtvFfmpegReader;
	bool isSoftYuv2Rgb;
	bool isNonPower2;
	bool isTestVideoDriver;
	float vArray[8];
#ifdef FPS_COUNTER
   QTime fpsTime;
   int nbFramesDisplayed;
#endif

    enum Program
    {
        YV12toRGB = 0,
        YUY2toRGB = 1,
        ProgramCount = 2
    };

	void updateAppLayerTexture(QWidget* widget);
    void updateTexture(QWidget* widget);
	void preinit();
    void checkGLPrograms(QWidget *target);

// extension prototypes
#ifndef Q_WS_MAC
# ifndef APIENTRYP
#   ifdef APIENTRY
#     define APIENTRYP APIENTRY *
#   else
#     define APIENTRY
#     define APIENTRYP *
#   endif
# endif
#else
# define APIENTRY
# define APIENTRYP *
#endif

    // ARB_fragment_program
    typedef void (APIENTRY *_glProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
    typedef void (APIENTRY *_glBindProgramARB) (GLenum, GLuint);
    typedef void (APIENTRY *_glDeleteProgramsARB) (GLsizei, const GLuint *);
    typedef void (APIENTRY *_glGenProgramsARB) (GLsizei, GLuint *);
    typedef void (APIENTRY *_glProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    typedef void (APIENTRY *_glActiveTexture) (GLenum);
    _glProgramStringARB glProgramStringARB;
    _glBindProgramARB glBindProgramARB;
    _glDeleteProgramsARB glDeleteProgramsARB;
    _glGenProgramsARB glGenProgramsARB;
    _glProgramLocalParameter4fARB glProgramLocalParameter4fARB;
    _glActiveTexture glActiveTexture;
    bool m_checkedPrograms;
    bool m_usingOpenGL;
	bool m_stopped;
    GLuint m_program[2];
    GLuint m_texture[3];

	bool m_textureInitialized;
	double m_wCoeff;
	double m_hCoeff;
	QVector<uchar> yuv2rgbBuffer;
	GLuint m_texture_appLayer;
	GLuint m_backgroundTexture;
	bool m_afterPlayFirstTextureCreated;
	bool m_updateFullAppLayer;
	bool m_appLayerTextureReady;
	bool m_videoTextureReady;
	int m_alWidth;
	int m_alHeight;
	int m_backgroundWidth;
	int m_backgroundheight;
	float m_videoCoeffW[4];
	float m_videoCoeffH[4];
	uint textureWidth;
	uint textureHeight;
	float Par;
	bool autoAspect;
	float sar;
	GLint clampConstant;
};

#ifdef _DEBUG
#define OGL_CHECK_ERROR(str) if (checkOpenGLErrorDebug() != GL_NO_ERROR) {qDebug() << Q_FUNC_INFO << __LINE__ << str; }
#else
#define OGL_CHECK_ERROR(str) if (checkOpenGLError() != GL_NO_ERROR) {qDebug() << Q_FUNC_INFO << __LINE__ << str; }
#endif

VideoRendererSoftFilter::VideoRendererSoftFilter(VideoRendererSoft *renderer, QWidget* widget) : m_renderer(renderer),
	m_width(0), m_height(0),
	arrayPixels(NULL),
	m_texture_appLayer(0),
	m_backgroundTexture(0),
	textureWidth(0),
	textureHeight(0),
	m_backgroundWidth(0),
	m_backgroundheight(0),
	appTextureWidth(0),
	appTextureHeight(0),
	Par(4.0f / 3.0f),
	sar(1.0f),//768.0f / 720.0f),
	autoAspect(true),
	qtvFfmpegReader(NULL),
	isSoftYuv2Rgb(false),
	clampConstant(GL_CLAMP),
	isNonPower2(false),
#ifndef ADDITION_TEST_VIDEO_DRIVER
	isTestVideoDriver(true)
#else
	isTestVideoDriver(false)
#endif
    ,m_usingOpenGL(false), m_checkedPrograms(false), m_textureUploaded(false)
{
    //simply initialize the array with default values
    applyMixerSettings(0., 0., 0., 0.);
	m_afterPlayFirstTextureCreated = true;
	m_updateFullAppLayer = true;
	m_appLayerTextureReady = false;
	m_videoTextureReady = false;
	m_stopped = true;
	m_alWidth = 1;
	m_alHeight = 1;
	m_backgroundWidth = m_backgroundheight = 0;
	checkGLPrograms(widget);
}

VideoRendererSoftFilter::~VideoRendererSoftFilter()
{
    //this frees up resources
    freeResources();
	glDeleteTextures(1, &m_texture_appLayer);
	OGL_CHECK_ERROR("glDeleteTextures");
	m_texture_appLayer = 0;
}

void VideoRendererSoftFilter::getTextureRect(QRect& drawRect, float textureWidth, float textureHeight,
	float windowWidth, float windowHeight, const float sar) const
{
	int newTextureWidth = static_cast<uint>(textureWidth * sar);
	float windowAspect = windowWidth / windowHeight;
	float textureAspect = newTextureWidth / textureHeight;
	if (windowAspect > textureAspect)
	{
		// black bars at left and right
		drawRect.setTop(0);
		drawRect.setHeight(static_cast<int>(windowHeight));
		float scale = windowHeight / static_cast<float>(textureHeight);
		float scaledWidth = newTextureWidth * scale;
		drawRect.setLeft(static_cast<int>(windowWidth - scaledWidth) / 2);
		drawRect.setWidth(static_cast<int>(scaledWidth));
	}
	else {
		// black bars at the top and bottom
		drawRect.setLeft(0);
		drawRect.setWidth(static_cast<int>(windowWidth));
		if (newTextureWidth < windowWidth) {
			float scale = windowWidth / (double) newTextureWidth;
			float scaledHeight = textureHeight * scale;
			drawRect.setTop(static_cast<int>((windowHeight - scaledHeight) / 2));
			drawRect.setHeight(static_cast<int>(scaledHeight));
		}
		else {
			int newTextureHeight = static_cast<int>(windowWidth / textureAspect);
			drawRect.setTop(static_cast<int>((windowHeight - newTextureHeight) / 2));
			drawRect.setHeight(newTextureHeight);
		}
	}
}

int VideoRendererSoftFilter::checkOpenGLError() const
{
	int err = glGetError();
	if (GL_NO_ERROR != err) {
		const char* errorString = reinterpret_cast<const char *>(gluErrorString(err));
		if (errorString) {
			qDebug() << "OpenGL Error: " << errorString << "\n";
		}
	}
	return err;
}

#ifdef _DEBUG
int VideoRendererSoftFilter::checkOpenGLErrorDebug() const
{
	int err = checkOpenGLError();
	Q_UNUSED(err);
	return err;
}
#endif

void VideoRendererSoftFilter::setTextureParameters() const
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	OGL_CHECK_ERROR("glTexParameteri");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	OGL_CHECK_ERROR("glTexParameteri");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	OGL_CHECK_ERROR("glTexParameteri");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	OGL_CHECK_ERROR("glTexParameteri");
}

QSize VideoRendererSoftFilter::videoSize() const
{
	return QSize(m_width, m_height);
}

void VideoRendererSoftFilter::Clear() const
{
	// Set Clear Color
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	OGL_CHECK_ERROR("glClearColor");
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	OGL_CHECK_ERROR("glClear");
}

#ifdef ADDITION_TEST_VIDEO_DRIVER
	struct FillTexture {
		typedef QVector<uchar> ucharArray;
		static void fillTexture(ucharArray& pixels, int width, int height, int offsetLeft, int total, uchar color)
		{
			if (offsetLeft) {
				for(int h = 0; h < height; ++h) {
					for(int w = 0; w < total; ++w) {
						pixels[h * width + w + offsetLeft] = color;
					}
				}
			}
			else {
				for(int h = 0; h < height; ++h) {
					for(int w = 0; w < total; ++w) {
						pixels[h * width + w] = color;
					}
				}
			}
		}
	};
void VideoRendererSoftFilter::SwapBuffers() const
{
#ifdef Q_OS_WIN32
	HDC hDc = wglGetCurrentDC();
	if (!hDc) {
		qDebug() << "wglGetCurrentDC: HDC == NULL, error code" << GetLastError();
	}
	else {
		BOOL res = ::SwapBuffers(hDc);
		if (!res) {
			qDebug() << "SwapBuffers error: " << GetLastError();
		}
	}
#elif Q_OS_MAC
	// glXSwapBuffers or other platform specific function call 
#elif Q_OS_LINUX
	// glXSwapBuffers or other platform specific function call 
#endif
}
	static const uchar YUV[] = {
		static_cast<uchar>((0.257 * 255) + (0.504 * 0) + (0.098 * 0) + 16),
		static_cast<uchar>(-(0.148 * 255) - (0.291 * 0) + (0.439 * 0) + 128),
		static_cast<uchar>((0.439 * 255) - (0.368 * 0) - (0.071 * 0) + 128),

		static_cast<uchar>((0.257 * 0) + (0.504 * 255) + (0.098 * 0) + 16),
		static_cast<uchar>(-(0.148 * 0) - (0.291 * 255) + (0.439 * 0) + 128),
		static_cast<uchar>((0.439 * 0) - (0.368 * 255) - (0.071 * 0) + 128),

		static_cast<uchar>((0.257 * 0) + (0.504 * 0) + (0.098 * 255) + 16),
		static_cast<uchar>(-(0.148 * 0) - (0.291 * 0) + (0.439 * 255) + 128),
		static_cast<uchar>((0.439 * 0) - (0.368 * 0) - (0.071 * 255) + 128),
	};


const uchar* VideoRendererSoftFilter::screenShoot(const char* fileName) const
{
	float viewPort[4] = {0.0f};
	glGetFloatv(GL_VIEWPORT, viewPort);
	OGL_CHECK_ERROR("glGetFloatv");
	int width = static_cast<int>(viewPort[2]);
	int height = static_cast<int>(viewPort[3]);
	enum {
		Depth = 4
	};
	static QVector<uchar> screenBuffer;
	// screen pixel buffer
	screenBuffer.resize(Depth * width * width);
	// grab screen pixel buffer
	glReadPixels(0, 0, width, height, 4 == Depth ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, &screenBuffer[0]);
	OGL_CHECK_ERROR("glReadPixels");
	const char* sceenShotFileName = fileName ? fileName : (4 == Depth ? "screen.png" : "screen.jpg");
	Q_UNUSED(sceenShotFileName);
	return !screenBuffer.empty() ? &screenBuffer[0] : NULL;
}

bool VideoRendererSoftFilter::checkVideoDriver()
{
	if (isSoftYuv2Rgb || isTestVideoDriver) {
		return true;
	}
	//isTestVideoDriver = true;
	//isSoftYuv2Rgb = true; // todo: delete me
	//initSoftYUV2RBG();
	//return true;

	qDebug() << "Start Video Driver Test...";
	static GLuint yTestTexture;
	static GLuint uvTestTexture[2] = {0};
	int width = m_width;
	int height = m_height;
	int halfWidth = width / 2;
	int halfHeight = height / 2;
	// first start
	if (!yTestTexture && !uvTestTexture[0] && !uvTestTexture[1]) {
		int wPow = isNonPower2 ? width : getMinPow2(width);
		int hPow = isNonPower2 ? height : getMinPow2(height);
		glGenTextures(1, &yTestTexture);
		OGL_CHECK_ERROR("glGenTextures");
		glBindTexture(GL_TEXTURE_2D, yTestTexture);
		OGL_CHECK_ERROR("glBindTexture");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, wPow, hPow, 0,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, isNonPower2 ? 0 : width);
		OGL_CHECK_ERROR("glPixelStorei");
		setTextureParameters();
		glGenTextures(2, uvTestTexture);
		OGL_CHECK_ERROR("glGenTextures");
		wPow = isNonPower2 ? halfWidth : getMinPow2(halfWidth);
		hPow = isNonPower2 ? halfHeight : getMinPow2(halfHeight);
		for(int i = 0; i < 2; ++i) {
			glBindTexture(GL_TEXTURE_2D, uvTestTexture[i]);
			OGL_CHECK_ERROR("glBindTexture");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, wPow, hPow, 0,
				GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
			OGL_CHECK_ERROR("glTexImage2D");
			glPixelStorei(GL_UNPACK_ROW_LENGTH, isNonPower2 ? 0 : halfWidth);
			OGL_CHECK_ERROR("glPixelStorei");
			setTextureParameters();
		}
	}

	// Start test. Run shader and three test textures
		// Fill Test Textures
	FillTexture::ucharArray pixelsY(width * height, 0);
	FillTexture::ucharArray pixelsU(halfWidth * halfHeight, 0);
	FillTexture::ucharArray pixelsV(halfWidth * halfHeight, 0);

	int offsetLeftY = width / 3;
	int totalY = width / 3;
	int offsetLeftUV = halfWidth / 3;
	int totalUV = halfWidth / 3;

	FillTexture::fillTexture(pixelsY, width, height, 0, totalY, YUV[0]);
	FillTexture::fillTexture(pixelsY, width, height, offsetLeftY, totalY, YUV[3]);
	FillTexture::fillTexture(pixelsY, width, height, 2 * offsetLeftY, totalY, YUV[6]);

	FillTexture::fillTexture(pixelsU, halfWidth, halfHeight, 0, totalUV, YUV[1]);
	FillTexture::fillTexture(pixelsU, halfWidth, halfHeight, offsetLeftUV, totalUV, YUV[4]);
	FillTexture::fillTexture(pixelsU, halfWidth, halfHeight, 2 * offsetLeftUV, totalUV, YUV[7]);

	FillTexture::fillTexture(pixelsV, halfWidth, halfHeight, 0, totalUV, YUV[2]);
	FillTexture::fillTexture(pixelsV, halfWidth, halfHeight, offsetLeftUV, totalUV, YUV[5]);
	FillTexture::fillTexture(pixelsV, halfWidth, halfHeight, 2 * offsetLeftUV, totalUV, YUV[8]);

	// Set Y Texture
	glBindTexture(GL_TEXTURE_2D, yTestTexture);
	OGL_CHECK_ERROR("glBindTexture");
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, &pixelsY[0]);
	OGL_CHECK_ERROR("glTexSubImage2D");

	// Set U Texture
	glBindTexture(GL_TEXTURE_2D, uvTestTexture[0]);
	OGL_CHECK_ERROR("glBindTexture");
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, halfWidth, halfHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, &pixelsU[0]);
	OGL_CHECK_ERROR("glTexSubImage2D");

	// Set V Texture
	glBindTexture(GL_TEXTURE_2D, uvTestTexture[1]);
	OGL_CHECK_ERROR("glBindTexture");
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, halfWidth, halfHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, &pixelsV[0]);
	OGL_CHECK_ERROR("glTexSubImage2D");
	Clear();
	SwapBuffers();
	// Set View Port
	glViewport(0, 0, width, height);
	OGL_CHECK_ERROR("glViewport");
	for(int i = 0; i < 2; ++i) {
		Clear();
		// Run Draw Texture for test textures
		drawVideoTexture(yTestTexture, uvTestTexture[0], uvTestTexture[1], vArray);
		glFlush();
		OGL_CHECK_ERROR("glFlush");
		glFinish();
		OGL_CHECK_ERROR("glFinish");
		SwapBuffers();
	}
	// Check Result
	const uchar* screen = screenShoot("screenshot.png");
	int offsets[] = {0, offsetLeftY, 2 * offsetLeftY};
	uchar TestR[] = {255, 0, 0 };
	uchar TestG[] = {0, 255, 0 };
	uchar TestB[] = {0, 0, 255 };
	for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[i]); ++i) {

		const uchar* pixels = screen + 4 * (offsets[i] + height / 2 * width + totalY / 2);
		const uchar R = pixels[0];
		const uchar G = pixels[1];
		const uchar B = pixels[2];
		enum {
			diff = 10
		};
		if (abs(TestR[i] - R) > diff) {
			qDebug() << "Test " << i << " TestR " << TestR[i] << " != R " << R << " Error convert R";
			isSoftYuv2Rgb = true;
		}
		if (abs(TestG[i] - G) > diff) {
			qDebug() << "Test " << i << " TestG " << TestG[i] << " != G " << G << " Error convert G";
			isSoftYuv2Rgb = true;
		}
		if (abs(TestB[i] - B) > diff) {
			qDebug() << "Test " << i << " TestB " << TestB[i] << " != B " << B << " Error convert B";
			isSoftYuv2Rgb = true;
		}

		if (isSoftYuv2Rgb) {
			initSoftYUV2RBG();
			qDebug() << "Wrong video driver. use soft yuv2rgb conversions";
			break;
		}
	}
	isTestVideoDriver = true;

	glDeleteTextures(1, &yTestTexture);
	yTestTexture = 0;

	OGL_CHECK_ERROR("glDeleteTextures");
	glDeleteTextures(2, uvTestTexture);
	memset(uvTestTexture, 0, sizeof(uvTestTexture));
	OGL_CHECK_ERROR("glDeleteTextures");
	return true;
}
#endif // ADDITION_TEST_VIDEO_DRIVER

void VideoRendererSoftFilter::preinit()
{
	m_usingOpenGL = true;
	//those "textures" will be used as byte streams
	//to pass Y, U and V data to the graphics card
	glGenTextures(3, m_texture);
	OGL_CHECK_ERROR("glGenTextures");
	glGenTextures(1, &m_texture_appLayer);
	OGL_CHECK_ERROR("glGenTextures");
	glGenTextures(1, &m_backgroundTexture);
	OGL_CHECK_ERROR("glGenTextures");
	enum {
		size = 256,
		color = 0xff000000
	};
	// Pixel Data Black color
	QVector<uint> pixels(size * size, color);
	glEnable(GL_TEXTURE_2D);
	OGL_CHECK_ERROR("glEnable");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
	OGL_CHECK_ERROR("glTexImage2D");
	setTextureParameters();
	m_textureInitialized = false;
	m_wCoeff = 1.0;
	m_hCoeff = 1.0;
}

void VideoRendererSoftFilter::initSoftYUV2RBG()
{
	isSoftYuv2Rgb = true;
	isTestVideoDriver = true;
}

void VideoRendererSoftFilter::checkGLPrograms(QWidget* target)
{
    if (!m_checkedPrograms) {
		QPainter painter(target);
		m_checkedPrograms = true;
        //we check only once if the widget is drawn using opengl
        glProgramStringARB = (_glProgramStringARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramStringARB"));
        glBindProgramARB = (_glBindProgramARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glBindProgramARB"));
        glDeleteProgramsARB = (_glDeleteProgramsARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glDeleteProgramsARB"));
        glGenProgramsARB = (_glGenProgramsARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glGenProgramsARB"));
        glProgramLocalParameter4fARB = (_glProgramLocalParameter4fARB) QGLContext::currentContext()->getProcAddress(QLatin1String("glProgramLocalParameter4fARB"));
        glActiveTexture = (_glActiveTexture) QGLContext::currentContext()->getProcAddress(QLatin1String("glActiveTexture"));

		if (!glActiveTexture) {
			qWarning() << "GL_ARB_multitexture not support";
		}
		// OpenGL info
		const char* extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
		if (extensions) {
			qWarning() << "OpenGL extensions: " << extensions;
		}
		const char* version = reinterpret_cast<const char *>(glGetString(GL_VERSION));

        if (version) {
			qWarning() << "OpenGL version: " << version;
		}

        const uchar* renderer = glGetString(GL_RENDERER);
		if (renderer) {
			qWarning() << "Renderer: " << reinterpret_cast<const char *>(renderer);
		}

        const uchar* vendor = glGetString(GL_VENDOR);
		if (vendor) {
			qWarning() << "Vendor: " << reinterpret_cast<const char *>(vendor);
		}

        //version = "1.0.7";
		if (version && QString(version) >= QString("1.2.0")) {
			clampConstant = GL_CLAMP_TO_EDGE;
		} else if (extensions && strstr(extensions, "GL_EXT_texture_edge_clamp")) {
			clampConstant = GL_CLAMP_TO_EDGE_EXT;
		} else if (extensions && strstr(extensions, "GL_SGIS_texture_edge_clamp")) {
			clampConstant = GL_CLAMP_TO_EDGE_SGIS;
		} else {
			clampConstant = GL_CLAMP;
		}

		if (version && QString(version) <= "1.1.0") { // Microsoft Generic software
			const QString& message = QObject::trUtf8("SLOW_OPENGL");
			qWarning() << message << "\n";
			QMessageBox* box = new QMessageBox(QMessageBox::Warning, "Info", message, QMessageBox::Ok, 0);
			box->show();
		}
		bool error = false;
		if (extensions) {
			isNonPower2 = strstr(extensions, "GL_ARB_texture_non_power_of_two") != NULL;
			const char* fragmentProgram = "GL_ARB_fragment_program";
			if (!strstr(extensions, fragmentProgram)) {
				qWarning() << fragmentProgram << " not support";
				error = true;
			}
		}
		if (!error && glProgramStringARB && glBindProgramARB && glDeleteProgramsARB &&
			glGenProgramsARB && glProgramLocalParameter4fARB) {
			glGenProgramsARB(2, m_program);


			glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[0]);
			checkOpenGLError();
			const GLbyte* gl_src = reinterpret_cast<const GLbyte *>(yv12ToRgb);
			glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
				sizeof(yv12ToRgb) - 1, gl_src);
			if (checkOpenGLError() != GL_NO_ERROR) {
				error = true;
			}
			if (error) {
				glDeleteProgramsARB(2, m_program);
				qWarning() << "Error compile shader!!!";
			}
		}
		else {
			error = true;
		}
		const QStringList& argList = QApplication::instance()->arguments();
		const QStringList::const_iterator it = argList.begin();
		bool isArgSoftYuv2Rgb = false;
		if (qFind(argList, "-softyuv2rgb") != argList.end()) {
			error = true;
			isArgSoftYuv2Rgb = true;
		}

		if (error) {
			if (isArgSoftYuv2Rgb) {
				qDebug() << "Use soft yuv->rgb converter";
			}
			else {
				qWarning() << "OpenGL shader support init failed, use soft yuv->rgb converter!!!";
			}
			initSoftYUV2RBG();
		}
		preinit();
	}
}

#ifdef Q_OS_MAC
/**
 * @brief Qt for Mac OS X uses system backing store, so we have to get
 * pixmap using Carbon API.
 */
CFDataRef qtv_macWidgetBackingStore(QWidget* widget, int* imageWidth, int *imageHeight)
{
//   widget->setUpdatesEnabled(false);
    HIViewRef viewRef = reinterpret_cast<HIViewRef>(widget->winId());

    HIRect frame;
    CGImageRef image = NULL;
    HIViewCreateOffscreenImage(viewRef, 0, &frame, &image);

    *imageWidth = frame.size.width - frame.origin.x;
    *imageHeight = frame.size.height - frame.origin.y;

    CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));

    CGImageRelease(image);

//    widget->setUpdatesEnabled(true);
    return data;
}
#endif



void  VideoRendererSoftFilter::updateAppLayerTexture(QWidget* widget)
{
	uint Aspect = widget->property("aspect").toUInt();
	autoAspect = !Aspect;
	if (!autoAspect) {
		setAspectRatio(Aspect);
	}
	uint mutexhandle = widget->property("ndsSyncMutex").toUInt();
	uint queueHandle = widget->property("ndsSyncQueue").toUInt();
	uint widgetHandle = widget->property("ndsWidget").toUInt();
	if (!qtvFfmpegReader) {
		qtvFfmpegReader = reinterpret_cast<QtvVideoDataReader *>(widget->property("QtvVideoDataReader").toUInt());
		if (qtvFfmpegReader) {
			qtvFfmpegReader->setUpdater(this);
		}
		textureMutex = reinterpret_cast<QMutex *>(widget->property("videoMutex").toUInt());
	}
	if (!widgetHandle || !mutexhandle || !queueHandle)
		return;
	QMutex* ndsSyncMutex = (QMutex*) (void*) mutexhandle;
	QQueue<QRect>* ndsSyncQueue = (QQueue<QRect>*) (void*) queueHandle;
	QRegion region;
	{
		QMutexLocker lock(ndsSyncMutex);
		while (ndsSyncQueue->size() > 0) {
			region += ndsSyncQueue->dequeue ();
		}
	}
	QRect paintRect = region.boundingRect();
	if (paintRect.width() > 0 && paintRect.height() > 0)  {
		QWidget* ndsWidget = (QWidget*) (void*) widgetHandle;

        unsigned char* imgBits;
        int imgWidth;
        int imgHeight;
        int imgDepth;


#ifdef Q_OS_MAC
        CFDataRef imageData = qtv_macWidgetBackingStore(ndsWidget, &imgWidth, &imgHeight);
        imgDepth = 32;
        imgWidth += 4;
//        imageBytesPerLine = (imgDepth >> 3) * imgWidth;
        imgBits = (unsigned char*)CFDataGetBytePtr(imageData);
#else
     	QWindowSurface* surface = ndsWidget->windowSurface();
#ifdef _DEBUG
		QImage* mainImg = dynamic_cast<QImage*> (surface->paintDevice());
#else
		QImage* mainImg = static_cast<QImage*> (surface->paintDevice());
#endif
		if (!mainImg) {
			return;
		}

        imgBits = mainImg->bits();
        imgWidth = mainImg->width();
        imgHeight = mainImg->height();
        imgDepth = mainImg->depth();
#endif


		glBindTexture(GL_TEXTURE_2D, m_texture_appLayer);
		OGL_CHECK_ERROR("glBindTexture")
		m_alWidth = imgWidth;
		m_alHeight = imgHeight;
		if (!m_textureInitialized) {
			appTextureWidth = imgWidth;
			appTextureHeight = imgHeight;
			textureWidth = getMinPow2(imgWidth);
			textureHeight = getMinPow2(imgHeight);
			int w = getMinPow2(imgWidth);
			int h = getMinPow2(imgHeight);
			m_wCoeff = (double) imgWidth / (double) w;
			m_hCoeff = (double) imgHeight / (double) h;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
				GL_BGRA, GL_UNSIGNED_BYTE, 0);
			OGL_CHECK_ERROR("glTexImage2D")
			m_textureInitialized = true;
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, imgWidth);
		OGL_CHECK_ERROR("glPixelStorei");
		paintRect.setY(0);
		paintRect.setHeight(imgHeight);
		if (m_updateFullAppLayer) {
			paintRect.setX(0);
			paintRect.setWidth(imgWidth);
			m_updateFullAppLayer = false;
		}
		// round Rect coordinates to 8 (some adapters does not recognize unallignment coordinate)
		QRect roundRect;
		roundRect.setX((paintRect.x() >> 3) << 3);
		roundRect.setY((paintRect.y() >> 3) << 3);
		roundRect.setWidth(paintRect.x() + paintRect.width() - roundRect.x());
		int rest = roundRect.width() % 8;
		if (rest)
			roundRect.setWidth(roundRect.width() + (8-rest));
		roundRect.setHeight(paintRect.y() + paintRect.height() - roundRect.y());
		rest = roundRect.height() % 8;
		if (rest)
			roundRect.setHeight(roundRect.height() + (8-rest));
		quint8* data = imgBits + (roundRect.y() * imgWidth + roundRect.x()) * (imgDepth >> 3);
		if (imgDepth == 32) {
			glTexSubImage2D(GL_TEXTURE_2D, 0,
				roundRect.x(), roundRect.y(),
				roundRect.width(), roundRect.height(),
				GL_BGRA, GL_UNSIGNED_BYTE, data);
		}
		else if (imgDepth == 16) {
			glTexSubImage2D(GL_TEXTURE_2D, 0,
				roundRect.x(), roundRect.y(),
				roundRect.width(), roundRect.height(),
				GL_RGB16, GL_UNSIGNED_BYTE, data);
		}
		OGL_CHECK_ERROR("glTexSubImage2D");
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		OGL_CHECK_ERROR("glPixelStorei");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampConstant);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampConstant);
		OGL_CHECK_ERROR("glTexParameteri");
		m_appLayerTextureReady = true;
#ifdef Q_OS_MAC
        CFRelease(imageData);
#endif
	}
	unsigned backgroundHandle = widget->property("ndsBackground").toUInt();
	if (backgroundHandle && m_updateFullAppLayer) {
		QImage* ndsBackground = (QImage*) (void*) backgroundHandle;
		glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
		OGL_CHECK_ERROR("glBindTexture");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ndsBackground->width(), ndsBackground->height(), 0,
			GL_BGRA, GL_UNSIGNED_BYTE, ndsBackground->bits());
		OGL_CHECK_ERROR("glTexImage2D");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		OGL_CHECK_ERROR("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		OGL_CHECK_ERROR("glTexParameteri");
		m_backgroundWidth = ndsBackground->width();
		m_backgroundheight = ndsBackground->height();
	}
}

void VideoRendererSoftFilter::updateTexture(QWidget* widget)
{
	// new image 
	updateAppLayerTexture(widget);
	if (qtvFfmpegReader) {
		m_stopped = !qtvFfmpegReader->isStop();
	}
	if (m_stopped) {
		return;
	}
	QMutexLocker locker(textureMutex);
	if (!m_width) {
		return;
	}
	if (!arrayPixels) {
		return;
	}
	const int w[3] = { m_width, m_width / 2, m_width / 2 };
	int h[3] = { m_height, m_height / 2, m_height / 2 };
	glEnable(GL_TEXTURE_2D);
	OGL_CHECK_ERROR("glEnable");
	if (!isSoftYuv2Rgb) {
		for (int i = 0; i < 3; ++i) {
			glBindTexture(GL_TEXTURE_2D, m_texture[i]);
			OGL_CHECK_ERROR("glBindTexture");
			const uchar* pixels = arrayPixels[i];
			if (!m_videoTextureReady) {
				// if support "GL_ARB_texture_non_power_of_two", use default size of texture,
				// else nearest power of two
				const int wPow = isNonPower2 ? w[i] : getMinPow2(w[i]);
				const int hPow = isNonPower2 ? h[i] : getMinPow2(h[i]);
				// support GL_ARB_texture_non_power_of_two ?
				if (isNonPower2) {
					m_videoCoeffW[i] = 1.0f;
					m_videoCoeffH[i] = 1.0f;
				}
				else {
					float wCoeff = w[i] / static_cast<float>(wPow);
					float hCoeff = h[i] / static_cast<float>(hPow);
					m_videoCoeffW[i] = wCoeff;
					m_videoCoeffH[i] = hCoeff;
				}
				glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, wPow, hPow, 0,
				GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
				OGL_CHECK_ERROR("glTexImage2D");
				glPixelStorei(GL_UNPACK_ROW_LENGTH, w[i]);
				OGL_CHECK_ERROR("glPixelStorei");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampConstant);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampConstant);
				OGL_CHECK_ERROR("glTexParameteri");
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0,
					0, 0,
					w[i], h[i],
					GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
			OGL_CHECK_ERROR("glTexSubImage2D");
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			OGL_CHECK_ERROR("glPixelStorei");				
		}
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_texture[0]);
		OGL_CHECK_ERROR("glBindTexture");
		const int wPow = isNonPower2 ? w[0] : getMinPow2(w[0]);
		const int hPow = isNonPower2 ? h[0] : getMinPow2(h[0]);
		if (!m_videoTextureReady) {
			if (isNonPower2) {
				m_videoCoeffW[0] = 1.0f;
				m_videoCoeffH[0] = 1.0f;
			}
			else {
				float wCoeff = w[0] / static_cast<float>(wPow);
				float hCoeff = h[0] / static_cast<float>(hPow);
				m_videoCoeffW[0] = wCoeff;
				m_videoCoeffH[0] = hCoeff;
			}
			if (!m_videoTextureReady) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wPow, hPow, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, 0);
				OGL_CHECK_ERROR("glTexImage2D");
				glPixelStorei(GL_UNPACK_ROW_LENGTH, isNonPower2 ? 0 : w[0]);
				OGL_CHECK_ERROR("glPixelStorei");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampConstant);
				OGL_CHECK_ERROR("glTexParameteri");
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampConstant);
				OGL_CHECK_ERROR("glTexParameteri");
			}
		}				
		int size = 4 * w[0] * h[0];
		if (yuv2rgbBuffer.size() < size) {
			yuv2rgbBuffer.resize(size);
			yuv2rgbBuffer.fill(255);
		}
		uchar* pixels = &yuv2rgbBuffer[0];
		yuv420_argb32_mmx(pixels, arrayPixels[0], arrayPixels[2], arrayPixels[1], w[0], h[0], 4 * w[0], w[0], w[0] / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0,
			w[0], h[0],
			GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		OGL_CHECK_ERROR("glTexSubImage2D");
	}
	m_videoTextureReady = true;
	m_textureUploaded = true;
	if (m_afterPlayFirstTextureCreated) {
		m_updateFullAppLayer = true;
		m_afterPlayFirstTextureCreated = false;
	}
}
void VideoRendererSoftFilter::drawTexture(GLuint texture, float wCoeff, float hCoeff, const float* vArray) const
{
	if (glActiveTexture) {
		glActiveTexture(GL_TEXTURE0);
	}
	float v_array[] = { 0, 0,
						m_renderer->target()->width(), 0,
						m_renderer->target()->width(), m_renderer->target()->height(),
						0, m_renderer->target()->height() };
	const float tx_array[8] = {0.0f, 0.0f, wCoeff, 0.0f, wCoeff, hCoeff, 0.0f, hCoeff};
	if (!vArray) {
		vArray = v_array;
	}
	const bool wasEnabled = glIsEnabled(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_2D);
	OGL_CHECK_ERROR("glEnable");
	glBindTexture(GL_TEXTURE_2D, texture);
	OGL_CHECK_ERROR("glBindTexture");
	glVertexPointer(2, GL_FLOAT, 0, vArray);
	OGL_CHECK_ERROR("glVertexPointer");
	glTexCoordPointer(2, GL_FLOAT, 0, tx_array);
	OGL_CHECK_ERROR("glTexCoordPointer");
	glEnableClientState(GL_VERTEX_ARRAY);
	OGL_CHECK_ERROR("glEnableClientState");
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	OGL_CHECK_ERROR("glEnableClientState");
	glDrawArrays(GL_QUADS, 0, 4);
	OGL_CHECK_ERROR("glDrawArrays");
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	OGL_CHECK_ERROR("glDisableClientState");
	glDisableClientState(GL_VERTEX_ARRAY);
	OGL_CHECK_ERROR("glDisableClientState");
	if (!wasEnabled) {
		glDisable(GL_TEXTURE_2D);
		OGL_CHECK_ERROR("glDisable");
	}
}
void VideoRendererSoftFilter::drawAppLayer(const QRect& r) const
{
	if (!appTextureHeight || !appTextureWidth)
		return;
	float width = appTextureWidth;

	const float sarGui = width == 720 ? 768.0f / 720.0f : 1.0f;
	float height = appTextureHeight;
	Q_ASSERT(textureWidth && "Zero Value");
	Q_ASSERT(textureHeight && "Zero Value");
	float tx = width / textureWidth;
	float ty = height / textureHeight;

	QRect temp;
	getTextureRect(temp, width, height, r.width(), r.height(), sarGui);

	//let's draw the texture
	const float v_array[] = { temp.left(), temp.top(), temp.right(), temp.top(), temp.right(), temp.bottom(), temp.left(), temp.bottom()};
	drawTexture(m_texture_appLayer, tx, ty, v_array);
}
void VideoRendererSoftFilter::drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array)
{
	
	float tx_array[8] = {0.0f, 0.0f, m_videoCoeffW[0], 0.0f, m_videoCoeffW[0], m_videoCoeffH[0],
		0.0f, m_videoCoeffH[0]};
	if (!isSoftYuv2Rgb) {
		const Program prog = YV12toRGB;
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_program[prog]);
		OGL_CHECK_ERROR("glBindProgramARB");
		//loading the parameters
		glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, m_brightness / 256.0f, m_contrast, cos(m_hue), sin(m_hue));
		OGL_CHECK_ERROR("glProgramLocalParameter4fARB");
		glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, m_saturation, painterOpacity, 0.0f, 0.0f);
		OGL_CHECK_ERROR("glProgramLocalParameter4fARB");
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		OGL_CHECK_ERROR("glEnable");

		if (YUY2toRGB == prog) {
			const float w = m_width / 2,
				iw = 1. / w;
			tx_array[3] = w;
			tx_array[6] = w;

			for (int i = 0; i < 4; ++i) {
				tx_array[3*i + 2] = iw;
			}
		}
		if (glActiveTexture) {
			glActiveTexture(GL_TEXTURE0);
			OGL_CHECK_ERROR("glActiveTexture");
		}
		glBindTexture(GL_TEXTURE_2D, tex0);
		OGL_CHECK_ERROR("glBindTexture");
		if (YV12toRGB == prog) {
			if (glActiveTexture) {
				glActiveTexture(GL_TEXTURE1);
				OGL_CHECK_ERROR("glActiveTexture");
			}
			glBindTexture(GL_TEXTURE_2D, tex1);
			OGL_CHECK_ERROR("glBindTexture");
			if (glActiveTexture) {
				glActiveTexture(GL_TEXTURE2);
				OGL_CHECK_ERROR("glActiveTexture");
			}
			glBindTexture(GL_TEXTURE_2D, tex2);
			OGL_CHECK_ERROR("glBindTexture");
		}
	}
	else {
		glBindTexture(GL_TEXTURE_2D, tex0);
		OGL_CHECK_ERROR("glBindTexture");
	}
	glVertexPointer(2, GL_FLOAT, 0, v_array);
	OGL_CHECK_ERROR("glVertexPointer");
	glTexCoordPointer(2, GL_FLOAT, 0, tx_array);
	OGL_CHECK_ERROR("glTexCoordPointer");
	glEnableClientState(GL_VERTEX_ARRAY);
	OGL_CHECK_ERROR("glEnableClientState");
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	OGL_CHECK_ERROR("glEnableClientState");
	glDrawArrays(GL_QUADS, 0, 4);
	OGL_CHECK_ERROR("glDrawArrays");
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	OGL_CHECK_ERROR("glDisableClientState");
	glDisableClientState(GL_VERTEX_ARRAY);
	OGL_CHECK_ERROR("glDisableClientState");
	if (!isSoftYuv2Rgb) {
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		OGL_CHECK_ERROR("glDisable");
	}
}
void VideoRendererSoftFilter::repaintCurrentFrame(QPainter &painter, const QRect &r)
{
	// paint event
    QMutexLocker locker(&m_mutex);
#ifdef FPS_COUNTER
	if (fpsTime.elapsed() > 1000) {
		qDebug("FPS_COUNTER: displayed=%d (%d)", nbFramesDisplayed, fpsTime.elapsed());
		nbFramesDisplayed = 0;
		fpsTime.restart();

	}
#endif
	videoRect = r;
	painterOpacity = painter.opacity();
#ifdef FPS_COUNTER
    nbFramesDisplayed++;
#endif

    if (painter.paintEngine()->type() == QPaintEngine::OpenGL) {
        //for now we only support YUV (both YV12 and YUY2)

        if (m_usingOpenGL) 
		{
			//let's draw the texture
			QRect temp;
			if (m_stopped) 
			{
				temp = r;
			}
			else 
			{
				if (m_width && m_height) 
				{
					float aspect = static_cast<float>(m_width) / m_height;
					if (autoAspect) 
					{
						Par = qtvFfmpegReader->sar() * aspect;
					}
					sar = Par / aspect;
					getTextureRect(temp, m_width, m_height, r.width(), r.height(), sar);
				}
			}
			const float v_array[] = { temp.left(), temp.top(), temp.right()+1, temp.top(), temp.right()+1, temp.bottom()+1, temp.left(), temp.bottom()+1 };

			//Let's pass the other arguments
			QSize vsize = m_renderer->videoSize();
			if (!m_stopped && vsize.width() > 0 && vsize.height() > 0) {
#ifdef ADDITION_TEST_VIDEO_DRIVER
				if (!isTestVideoDriver) {
					memcpy(vArray, v_array, sizeof(v_array));
				}
				if (checkVideoDriver()) 
#endif
				{
					updateTexture(m_renderer->target());
					drawVideoTexture(m_texture[0], m_texture[1], m_texture[2], v_array);
					if (m_appLayerTextureReady)
						drawAppLayer(r);
				}
			}
			else {
				updateTexture(m_renderer->target());
				drawTexture(m_backgroundTexture, 1.0, 1.0);
				if (m_appLayerTextureReady)
					drawAppLayer(r);
			}

		}

    } else if (m_usingOpenGL) {
        //we just switched to software again
        m_usingOpenGL = false;
        freeGLResources();
    }
}
bool VideoRendererSoftFilter::update(const uchar** pixels, uint width, uint height, int* stride)
{
	Q_UNUSED(stride);
	m_width = stride[0];
	
	m_height = height;
	arrayPixels = pixels;
	QApplication::postEvent(m_renderer, new QEvent(QEvent::UpdateRequest));
	return true;
}
void VideoRendererSoftFilter::reset()
{
	m_height = 0;
	m_width = 0;
	arrayPixels = NULL;
	m_videoTextureReady = false;
}
void VideoRendererSoftFilter::setAspectRatio(uint ar)
{
	static const float aspects[] = {
		1.0f,
		4.0f / 3.0f,
		16.0f / 9.0f
	};
	Q_ASSERT(ar < sizeof(aspects) / sizeof(aspects[0]));
	Par = aspects[ar];
}

//==============================================================================
//==============================================================================

VideoRendererSoft::VideoRendererSoft(QWidget *target) :
m_renderer(new VideoRendererSoftFilter(this, target)), m_target(target),
	 m_dstX(0), m_dstY(0), m_dstWidth(0), m_dstHeight(0)
{
}

VideoRendererSoft::~VideoRendererSoft()
{
}

void VideoRendererSoft::repaintCurrentFrame(QWidget *target, const QRect &rect)
{
    QPainter painter(target);

    QColor backColor = target->palette().color(target->backgroundRole());
    painter.setBrush(backColor);
    painter.setPen(Qt::NoPen);

    if (!m_videoRect.contains(rect)) {
        //we repaint the borders only when needed
        QRegion reg = QRegion(rect) - m_videoRect;
        foreach(QRect r, reg.rects()) {
            painter.drawRect(r);
        }
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setTransform(m_transform, true);
    m_renderer->repaintCurrentFrame(painter, m_videoRect);
}

void VideoRendererSoft::notifyResize(const QRect &rect)
{
    QSize vsize = videoSize();
	vsize = QSize(rect.width(), rect.height());
    internalNotifyResize(rect.size(), vsize);
    m_transform.reset();

    if (vsize.isValid() && rect.isValid()) {
        m_transform.translate(m_dstX, m_dstY);
        const qreal sx = qreal(m_dstWidth) / qreal(vsize.width()),
            sy = qreal(m_dstHeight) / qreal(vsize.height());
        m_transform.scale(sx, sy);
		m_videoRect = m_transform.mapRect( QRect(0,0, vsize.width(), vsize.height()));
    }
}

QSize VideoRendererSoft::videoSize() const
{
	Q_ASSERT(m_renderer && "NULL Pointer");
	return m_renderer->videoSize();
}

void VideoRendererSoft::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    m_renderer->applyMixerSettings(brightness, contrast, hue, saturation);
}

bool VideoRendererSoft::event(QEvent *e)
{
    if (e->type() == QEvent::UpdateRequest) {
        m_target->update(m_videoRect);
        return true;
    }
    return QObject::event(e);
}

void VideoRendererSoft::internalNotifyResize(const QSize &size, const QSize &videoSize)
{
	m_dstWidth = size.width();
	m_dstHeight = size.height();
	m_dstY = 0;
	qreal ratio = qreal(videoSize.width()) / qreal(videoSize.height());
	m_dstWidth = qRound(size.height() * ratio);
	m_dstX = qRound((size.width() - size.height() * ratio) / 2.);
}
