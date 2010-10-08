#ifndef animated_show_h1223
#define animated_show_h1223

#include <QTimer>
#include <QPointF>

struct Show_helper
{
	Show_helper();


	QTimer mTimer;
	int counrer;
	bool showTime;
	QPointF center;
	int radius;

	unsigned int value;
	bool positive_dir;
};

#endif //animated_show_h1223