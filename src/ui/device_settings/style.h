#ifndef settings_style_h_1447
#define settings_style_h_1447

class ArthurStyle : public QWindowsStyle
{
public:
	ArthurStyle();

	void drawHoverRect(QPainter *painter, const QRect &rect) const;

	void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
		QPainter *painter, const QWidget *widget = 0) const;
	//     void drawControl(ControlElement element, const QStyleOption *option,
	//                      QPainter *painter, const QWidget *widget) const;
	void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
		QPainter *painter, const QWidget *widget) const;
	QSize sizeFromContents(ContentsType type, const QStyleOption *option,
		const QSize &size, const QWidget *widget) const;

	QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const;
	QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt,
		SubControl sc, const QWidget *widget) const;

	//     SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option,
	// 				     const QPoint &pos, const QWidget *widget = 0) const;

	int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;

	void polish(QPalette &palette);
	void polish(QWidget *widget);
	void unpolish(QWidget *widget);
};

#endif //settings_style_h_1447
