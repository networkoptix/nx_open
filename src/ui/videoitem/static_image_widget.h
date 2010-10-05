#ifndef static_image_widget_h_1635
#define static_image_widget_h_1635

#include <QPixmap>
#include <QGLWidget>

class CLStaticImageWidget : public QGLWidget 
{
	Q_OBJECT
public:
	CLStaticImageWidget(QWidget *parent, QString img, QString name, int max_width);
signals:
	void onPressed(QString name);
protected:
	void paintEvent(QPaintEvent *event);
	void mouseReleaseEvent ( QMouseEvent * event );
protected:
	QPixmap m_img;
	QString m_name;

	int m_width;
	int m_height;

};

#endif//static_image_widget_h_1635