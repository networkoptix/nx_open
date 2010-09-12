#ifndef settings_widget_factory_h_1801
#define settings_widget_factory_h_1801

#include "device\param.h"

class QWidget;
class QObject;
class QLayout;


class CLSettingsWidgetFactory
{
public:
	static QWidget* getWidget(CLParam& param, QObject* handler);
	static QLayout* getLayout(CLParam& param, QObject* handler);
private:
	
	static QWidget* getOnOffWidget(CLParam& param, QObject* handler);
	static QWidget* getMinMaxStepWidget(CLParam& param, QObject* handler);
};


#endif // settings_widget_factory_h_1801