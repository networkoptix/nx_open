#ifndef camera_settings_widgets_h_1214
#define camera_settings_widgets_h_1214

#include <QtGui/QSlider>
#include "plugins/resources/camera_settings/camera_settings.h"


class QnSettingsGroupBox : public QGroupBox
{
    bool alreadyShowed;

    QnSettingsGroupBox();

public:

    QnSettingsGroupBox(const QString& title, QWidget* parent);

protected:

    void showEvent(QShowEvent* event) override;
};

//==============================================
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

class QnAbstractSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj, QnSettingsGroupBox& parent, const QString& hint);
    virtual ~QnAbstractSettingsWidget();

    virtual QWidget* toWidget();

    const CameraSetting& param() const;

signals:
    void setAdvancedParam(const CameraSetting& val);

public slots:
    virtual void updateParam(QString val) = 0;

protected:
    virtual void setParam(const CameraSettingValue& val);
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
protected:
    CameraSetting& mParam;
    QObject* mHandler;
    QWidget* mWidget;
    QHBoxLayout *mlayout;
    QString m_hint;
};
//==============================================
class QnSettingsOnOffWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QnSettingsGroupBox& parent);
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
    QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QnSettingsGroupBox& parent);

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
    QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QnSettingsGroupBox& parent);

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
    QnSettingsButtonWidget(QObject* handler, const CameraSetting& obj, QnSettingsGroupBox& parent);

public slots:
    void updateParam(QString val);

private slots:
    void onClicked();

private:
    CameraSetting dummyVal;
};

#endif //camera_settings_widgets_h_1214
