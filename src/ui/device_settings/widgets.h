#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include "device\param.h"
class CLDevice;
class QGroupBox;

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
	CLAbstractSettingsWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname);
	virtual ~CLAbstractSettingsWidget();

	virtual QWidget* toWidget();

	CLDevice* getDevice() const;
	const CLParam& param() const;

    QString group() const;
    QString subGroup() const;

signals:
	void setParam(const QString& name, const CLValue& val);

public slots:
    virtual void updateParam(CLValue val)
    {
        //cl_log.log("updateParam", cl_logALWAYS);
    }

protected:
	virtual void setParam_helper(const QString& name, const CLValue& val);
protected:
	CLDevice* mDevice;
	CLParam mParam;
	QObject* mHandler;
	QWidget* mWidget;
    QString m_group;
    QString m_sub_group;
};
//==============================================
class SettingsOnOffWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsOnOffWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname);
	~SettingsOnOffWidget();

public slots:
     void updateParam(CLValue val);

private slots:
	void stateChanged (int state);
private:
    QCheckBox * m_checkBox;
};
//==============================================
class SettingsMinMaxStepWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsMinMaxStepWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname);

public slots:
    void updateParam(CLValue val);

private slots:
	void onValChanged();
	void onValChanged(int val);

private:
	SettingsSlider* slider;
	QGroupBox* groupBox;
};
//==============================================
class SettingsEnumerationWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsEnumerationWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname);

public slots:
    void updateParam(CLValue val);

private slots:
	void onClicked();

};

//==============================================
class SettingsButtonWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsButtonWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname);

public slots:
     void updateParam(CLValue val);

private slots:
		void onClicked();

};

#endif //settings_widgets_h_1820