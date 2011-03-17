#ifndef clgl_renderer_12_29
#define clgl_renderer_12_29

#include "abstractrenderer.h"

class CLVideoWindowItem;

class CLGLRenderer : public CLAbstractRenderer
{
public:

	enum CLGLDrawHardwareStatus
	{
		CL_GL_NOT_TESTED,
		CL_GL_SUPPORTED,
		CL_GL_NOT_SUPPORTED
	};

	static void clearGarbage();

	CLGLRenderer(CLVideoWindowItem *vw);
	~CLGLRenderer();

	int getMaxTextureSize() const;

	void draw(CLVideoDecoderOutput& image, unsigned int channel);

	bool paintEvent(const QRect& r);

	virtual void beforeDestroy();

	QSize sizeOnScreen(unsigned int channel) const;

	void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
	{
		//let's normalize the values
		m_brightness = brightness * 128;
		m_contrast = contrast + 1.;
		m_hue = hue * 180.;
		m_saturation = saturation + 1.;
	}

	void copyVideoDataBeforePainting(bool copy);

private:

	void init(bool msgbox);
	static int gl_status; 

private:

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

	int checkOpenGLError() const;

	int getMinPow2(int value) const
	{
		int result = 1;
		while (value > result)
			result<<=1;

		return result;
	}

	void getTextureRect(QRect& drawRect, 
		float textureWidth, float textureHeight,
		float windowWidth, float windowHeight, const float sar) const;

	void drawVideoTexture(GLuint tex0, GLuint tex1, GLuint tex2, const float* v_array);
	void updateTexture();

private:
	GLint clampConstant;
	bool isNonPower2;
	bool isSoftYuv2Rgb;

	GLuint m_program[2];
	GLuint m_texture[3];

	bool m_textureUploaded;

	int m_stride, // in memorry 
		m_width, // visible width
		m_height,

		m_stride_old,
		m_height_old;

	unsigned char*  m_arrayPixels[3];

	CLColorSpace m_color, m_color_old; 

	enum Program
	{
		YV12toRGB = 0,
		YUY2toRGB = 1,
		ProgramCount = 2
	};

	float m_videoCoeffL[4];
	float m_videoCoeffW[4];
	float m_videoCoeffH[4];

	bool m_videoTextureReady;

	qreal m_brightness,
		m_contrast,
		m_hue,
		m_saturation,
		m_painterOpacity;

	mutable QMutex m_mutex; // to avoid call PaintEvent more than once at the same time.
	QWaitCondition m_waitCon;
	bool m_gotnewimage;

	bool m_needwait;

	CLVideoWindowItem* m_videowindow;

	bool m_inited;

	CLVideoDecoderOutput m_image;
	bool m_abort_drawing;

	bool m_do_not_need_to_wait_any_more;

	static QList<GLuint*> mGarbage;

	GLint mTexSize;

};

#endif //clgl_renderer_12_29
