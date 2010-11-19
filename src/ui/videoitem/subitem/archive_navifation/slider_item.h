#ifndef direct_jump_slider_h1607
#define direct_jump_slider_h1607

#include <QSlider>

class CLDirectJumpSlider : public QSlider
{
public:
	explicit CLDirectJumpSlider(Qt::Orientation orientation, QWidget * parent = 0 );

	~CLDirectJumpSlider();
protected:
	
	void mousePressEvent ( QMouseEvent * ev );

};

#endif //direct_jump_slider_h1607