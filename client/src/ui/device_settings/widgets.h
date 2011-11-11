#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include "core/resource/resource.h"




class QnResource;
class QGroupBox;

class SettingsSlider : public QSlider
{
	Q_OBJECT
public:
	SettingsSlider( Qt::Orientation orientation, QWidget * parent = 0 );

Q_SIGNALS:
    void onKeyReleased();

protected:
	void keyReleaseEvent ( QKeyEvent * event );
};

//==============================================

class CLAbstractSettingsWidget : public QObject
{
	Q_OBJECT
public:
	CLAbstractSettingsWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname);
	virtual ~CLAbstractSettingsWidget();

	virtual QWidget* toWidget();

	QnResourcePtr getDevice() const;
	const QnParam& param() const;

    QString group() const;
    QString subGroup() const;

signals:
	void setParam(const QString& name, const QnValue& val);

public slots:
    virtual void updateParam(QString val) = 0;

protected:
	virtual void setParam_helper(const QString& name, const QnValue& val);
protected:
	QnResourcePtr mDevice;
	QnParam& mParam;
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
	SettingsOnOffWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname);
	~SettingsOnOffWidget();

public slots:
     void updateParam(QString val);

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
	SettingsMinMaxStepWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname);

public slots:
    void updateParam(QString val);

private slots:
	void onValChanged();
	void onValChanged(int val);

private:
	SettingsSlider* m_slider;
	QGroupBox* groupBox;
};
//==============================================
class SettingsEnumerationWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsEnumerationWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname);

public slots:
    void updateParam(QString val);

private slots:
	void onClicked();
private:
    QRadioButton* getBtnByname(const QString& name);
private:
    QList<QRadioButton*> m_radioBtns;
};

//==============================================
class SettingsButtonWidget : public CLAbstractSettingsWidget
{
	Q_OBJECT
public:
	SettingsButtonWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname);

public slots:
     void updateParam(QString val);

private slots:
		void onClicked();

};

#endif //settings_widgets_h_1820
