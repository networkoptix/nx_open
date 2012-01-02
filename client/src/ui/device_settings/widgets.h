#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include <QtGui/QSlider>

#include <core/resource/resource.h>

class QCheckBox;
class QGroupBox;
class QRadioButton;

class QnResource;

class SettingsSlider : public QSlider
{
    Q_OBJECT

public:
    explicit SettingsSlider(Qt::Orientation orientation, QWidget *parent = 0);

Q_SIGNALS:
    void onKeyReleased();

protected:
    void keyReleaseEvent(QKeyEvent *event);
};

//==============================================

class CLAbstractSettingsWidget : public QObject
{
    Q_OBJECT

public:
    CLAbstractSettingsWidget(QnResourcePtr resource, const QnParam &param);
    virtual ~CLAbstractSettingsWidget();

    QnResourcePtr resource() const;
    const QnParam &param() const;

    QString group() const;
    QString subGroup() const;

    QWidget *widget() const;

Q_SIGNALS:
    void setParam(const QString &name, const QVariant &value);

protected:
    void setParamValue(const QVariant &value);

    virtual QWidget *createWidget() = 0;

    virtual void updateParam(const QVariant &value) = 0;

private:
    Q_DISABLE_COPY(CLAbstractSettingsWidget)

    const QnResourcePtr m_resource;
    QnParam m_param;
    mutable QWidget *m_widget;
};
//==============================================

class SettingsOnOffWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsOnOffWidget(QnResourcePtr dev, const QnParam &param);

protected:
    QWidget *createWidget();

    void updateParam(const QVariant &value);

private Q_SLOTS:
    void toggled(bool checked);

private:
    QCheckBox *m_checkBox;
};
//==============================================

class SettingsMinMaxStepWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsMinMaxStepWidget(QnResourcePtr dev, const QnParam &param);

protected:
    QWidget *createWidget();

    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onValueChanged();
    void onValueChanged(int value);

private:
    SettingsSlider *m_slider;
    QGroupBox *m_groupBox;
};
//==============================================

class SettingsEnumerationWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsEnumerationWidget(QnResourcePtr dev, const QnParam &param);

protected:
    QWidget *createWidget();

    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onClicked();

private:
    QRadioButton *getButtonByname(const QString &name) const;

    QList<QRadioButton *> m_radioButtons;
};

//==============================================
class SettingsButtonWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsButtonWidget(QnResourcePtr dev, const QnParam &param);

protected:
    QWidget *createWidget();

    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onClicked();
};

#endif //settings_widgets_h_1820
