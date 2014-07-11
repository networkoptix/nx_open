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
    QnSettingsScrollArea();

public:

    QnSettingsScrollArea(QWidget* parent);
    void addWidget(QWidget& widget);
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
    QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent, const QString& hint);
    virtual ~QnAbstractSettingsWidget();

    const CameraSetting& param() const;

    virtual void refresh() = 0;

signals:
    void setAdvancedParam(const CameraSetting& val);

public slots:
    virtual void updateParam(QString val) = 0;

protected:
    virtual void setParam(const CameraSettingValue& val);

protected:
    CameraSetting& m_param;
    QObject* m_handler;
    QHBoxLayout *m_layout;
    QString m_hint;
};
//==============================================
class QnSettingsOnOffWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent);
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
    QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent);

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
    QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent);

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
    QnSettingsButtonWidget(QObject* handler, const CameraSetting& obj, QnSettingsScrollArea& parent);

    void refresh() override;

public slots:
    void updateParam(QString val);

private slots:
    void onClicked();

private:
    CameraSetting m_dummyVal;
};

//==============================================
class QnSettingsTextFieldWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsTextFieldWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent);

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
    QnSettingsControlButtonsPairWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent);

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
