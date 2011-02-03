#ifndef direct_jump_slider_h1607
#define direct_jump_slider_h1607


class CLDirectJumpSlider : public QSlider
{
	//Q_OBJECT
public:
	explicit CLDirectJumpSlider(Qt::Orientation orientation, QWidget * parent = 0 );
	~CLDirectJumpSlider();

//signals:
	

protected:
	
	void mousePressEvent ( QMouseEvent * ev );

};

#endif //direct_jump_slider_h1607