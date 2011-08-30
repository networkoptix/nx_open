#ifndef OXYGEN_DEFS_H
#define OXYGEN_DEFS_H

#define CLAMP(x,l,u) (x) < (l) ? (l) :\
(x) > (u) ? (u) :\
(x)

#define _IsNotHtmlWidget(w) ( w->objectName() != "__khtml" )
#define _IsHtmlWidget(w) ( w->objectName() == "__khtml" )
#define _IsViewportChild(w) w->parent() &&\
( w->parent()->objectName() == "qt_viewport" || \
  w->parent()->objectName() == "qt_clipped_viewport" )

#define _HighContrastColor(c) (qGray(c.rgb()) < 128 ) ? Qt::white : Qt::black

#define _IsTabStack(w) ( w->objectName() == "qt_tabwidget_stackedwidget" )

#define SAVE_PEN QPen saved_pen = painter->pen();
#define RESTORE_PEN painter->setPen(saved_pen);
#define SAVE_BRUSH QBrush saved_brush = painter->brush();
#define RESTORE_BRUSH painter->setBrush(saved_brush);
#define SAVE_ANTIALIAS bool hadAntiAlias = painter->renderHints() & QPainter::Antialiasing;
#define RESTORE_ANTIALIAS painter->setRenderHint(QPainter::Antialiasing, hadAntiAlias);

#define RECT option->rect
#define PAL option->palette
#define COLOR(_ROLE_) PAL.color(_ROLE_)
#define CONF_COLOR(_TYPE_, _FG_) PAL.color(Style::config._TYPE_##_role[_FG_])
#define CCOLOR(_TYPE_, _FG_) PAL.color(Style::config._TYPE_##_role[_FG_])
#define FCOLOR(_TYPE_) PAL.color(QPalette::_TYPE_)
#define BGCOLOR (widget ? COLOR(widget->backgroundRole()) : FCOLOR(Window))
#define FGCOLOR (widget ? COLOR(widget->foregroundRole()) : FCOLOR(WindowText))
#define GRAD(_TYPE_) Style::config._TYPE_.gradient
#define ROLES(_TYPE_) QPalette::ColorRole (*role)[2] = &Style::config._TYPE_##_role
#define ROLE (*role)

#define ASSURE_OPTION(_VAR_, _TYPE_) \
const QStyleOption##_TYPE_ *_VAR_ = qstyleoption_cast<const QStyleOption##_TYPE_ *>(option);\
if (!_VAR_) return

#define HAVE_OPTION(_VAR_, _TYPE_)\
(const QStyleOption##_TYPE_ *_VAR_ = qstyleoption_cast<const QStyleOption##_TYPE_ *>(option))

#define ASSURE_WIDGET(_VAR_, _TYPE_)\
const _TYPE_ *_VAR_ = qobject_cast<const _TYPE_*>(widget);\
if (!_VAR_) return

#define ASSURE(_VAR_) if (!_VAR_) return

#define F(_I_) Dpi::target.f##_I_

#define IS_HTML_WIDGET (widget->objectName() == "RenderFormElementWidget")

#endif //OXYGEN_DEFS_H
