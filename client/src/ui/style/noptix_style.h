#ifndef QN_NOPTIX_STYLE_H
#define QN_NOPTIX_STYLE_H

#include <QProxyStyle>

class QnNoptixStyleAnimator;

class QnNoptixStyle: public QProxyStyle { 
    Q_OBJECT;

    typedef QProxyStyle base_type;

public:
    QnNoptixStyle(QStyle *style = NULL);

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;

    virtual int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const override;

    virtual void polish(QApplication *application) override;
    virtual void unpolish(QApplication *application) override;
    virtual void polish(QWidget *widget) override;
    virtual void unpolish(QWidget *widget) override;

protected:
    bool drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;

private:
    QnNoptixStyleAnimator *m_animator;
};


namespace {
    const char *hideCheckBoxInMenuPropertyName = "_qn_hideCheckBoxInMenu";
}

#endif // QN_NOPTIX_STYLE_H
