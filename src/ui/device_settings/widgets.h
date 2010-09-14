#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include <QSlider>

class SettingsSlider : public QSlider
{
	Q_OBJECT
public:
	SettingsSlider( Qt::Orientation orientation, QWidget * parent = 0 );
protected:
	void keyReleaseEvent ( QKeyEvent * event );
signals:
	void onKeyReleased();

};

#endif //settings_widgets_h_1820