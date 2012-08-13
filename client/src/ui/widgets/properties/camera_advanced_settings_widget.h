#ifndef camera_settings_widgets_h_1214
#define camera_settings_widgets_h_1214

#include <QtGui/QSlider>
#include "plugins/resources/camera_settings/camera_settings.h"

//#include "device/param.h"
//#include "base/log.h"
//class QnDevice;
class QGroupBox;

class QnSettingsSlider : public QSlider
{
    Q_OBJECT

public:
    QnSettingsSlider(Qt::Orientation orientation, QWidget *parent = 0);

Q_SIGNALS:
    void onKeyReleased();

protected:
    void keyReleaseEvent(QKeyEvent *event);
};

//==============================================

class QnAbstractSettingsWidget : public QWidget //public QObject
{
    Q_OBJECT
public:
    QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj);
    virtual ~QnAbstractSettingsWidget();

    virtual QWidget* toWidget();

    const CameraSetting& param() const;

signals:
    void setParam(const QString& name, const CameraSettingValue& val);

    public slots:
        virtual void updateParam(QString val) = 0;

protected:
    virtual void setParam_helper(const QString& name, const CameraSettingValue& val);
protected:
    CameraSetting& mParam;
    QObject* mHandler;
    QWidget* mWidget;
};
//==============================================
class QnSettingsOnOffWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QWidget& parent);
    ~QnSettingsOnOffWidget();

    public slots:
        void updateParam(QString val);

        private slots:
            void stateChanged (int state);
private:
    QCheckBox * m_checkBox;
};
//==============================================
class QnSettingsMinMaxStepWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QWidget& parent);

    public slots:
        void updateParam(QString val);

        private slots:
            void onValChanged();
            void onValChanged(int val);

private:
    QnSettingsSlider* m_slider;
    QGroupBox* groupBox;
};
//==============================================
class QnSettingsEnumerationWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QWidget& parent);

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
class QnSettingsButtonWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsButtonWidget(QObject* handler, QWidget& parent);

    public slots:
        void updateParam(QString val);

        private slots:
            void onClicked();

private:
    CameraSetting dummyVal;
};

#endif //camera_settings_widgets_h_1214
