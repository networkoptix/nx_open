#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include <QSlider>
#include "device\param.h"
class CLDevice;

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

//==============================================

class CLAbstractSettingsWidget : public QObject
{
	Q_OBJECT
public:
	CLAbstractSettingsWidget(QObject* handler, CLDevice*dev, QString paramname);
	virtual ~CLAbstractSettingsWidget();

	virtual QWidget* toWidget();

	CLDevice* getDevice() const;
	const CLParam& param() const;
signals:
	void setParam(const QString& name, const CLValue& val);

protected:
	CLDevice* mDevice;
	CLParam mParam;
	QObject* mHandler;
	QWidget* mWidget;

};
//==============================================
class SettingsOnOffWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsOnOffWidget(QObject* handler, CLDevice*dev, QString paramname);
	~SettingsOnOffWidget();
private slots:
	void stateChanged (int state);
};

class SettingsMinMaxStepWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsMinMaxStepWidget(QObject* handler, CLDevice*dev, QString paramname);
private slots:
	void onValChanged();

private:
	SettingsSlider* slider;
};

#endif //settings_widgets_h_1820