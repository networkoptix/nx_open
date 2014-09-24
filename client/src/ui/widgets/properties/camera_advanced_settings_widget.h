#ifndef camera_settings_widgets_h_1214
#define camera_settings_widgets_h_1214

#include <QtWidgets/QRadioButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSlider>
#include <QtWidgets/QScrollArea>
#include "plugins/resource/camera_settings/camera_settings.h"


class QnSettingsScrollArea : public QScrollArea
{
    Q_OBJECT;
public:
    explicit QnSettingsScrollArea(QWidget* parent);
    void addWidget(QWidget *widget);
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
    QnAbstractSettingsWidget(const CameraSetting &obj, QnSettingsScrollArea *parent, const QString &hint);
    virtual ~QnAbstractSettingsWidget();

    CameraSetting param() const;

    virtual void refresh() = 0;

signals:
    void advancedParamChanged(const CameraSetting& val);

public slots:
    virtual void updateParam(QString val) = 0;

protected:
    virtual void setParam(const CameraSettingValue& val);

protected:
    CameraSetting m_param;
    QHBoxLayout *m_layout;
    QString m_hint;
};
//==============================================
class QnSettingsOnOffWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsOnOffWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);
    ~QnSettingsOnOffWidget();

    void refresh() override;

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
    QnSettingsMinMaxStepWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);

    void refresh() override;

public slots:
    void updateParam(QString val);

    private slots:
        void onValChanged();
        void onValChanged(int val);

private:
    QnSettingsSlider* m_slider;
    QGroupBox* m_groupBox;
};
//==============================================
class QnSettingsEnumerationWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsEnumerationWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);

    void refresh() override;

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
    QnSettingsButtonWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);

    void refresh() override;

public slots:
    void updateParam(QString val);

private slots:
    void onClicked();
};

//==============================================
class QnSettingsTextFieldWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsTextFieldWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);

    void refresh() override;

public slots:
    void updateParam(QString val);

private slots:
    void onChange();

private:
    QLineEdit* m_lineEdit;
};

//==============================================
class QnSettingsControlButtonsPairWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsControlButtonsPairWidget(const CameraSetting &obj, QnSettingsScrollArea *parent);

    void refresh() override;

public slots:
    void updateParam(QString val);

private slots:
    void onMinPressed();
    void onMinReleased();
    void onMaxPressed();
    void onMaxReleased();

private:
    QPushButton* m_minBtn;
    QPushButton* m_maxBtn;
};

#endif //camera_settings_widgets_h_1214
