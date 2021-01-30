#include <QLayout>
#include <QDialog>

#include "mandelbrot_viewer.h"
#include "mapper_widget.h"

mandelbrot_viewer::mandelbrot_viewer(QWidget *parent) : QMainWindow(parent), dlg(this), widget(this)
{
  ui.setupUi(this);
  setCentralWidget(&widget);
  connect(&dlg, &mandelbrot_settings_dialog::draft_level_changed, this, &mandelbrot_viewer::on_draftLevelChanged);
  QMetaObject::invokeMethod(this, "on_draftLevelChanged", Qt::QueuedConnection,
                            Q_ARG(int, dlg.get_draft_level()));
}

void mandelbrot_viewer::on_settings()
{
  dlg.show();
}

void mandelbrot_viewer::on_draftLevelChanged(int new_draft_level)
{
  QMetaObject::invokeMethod(&widget, "change_draft_mip_level_event", Qt::QueuedConnection,
                            Q_ARG(int, new_draft_level));
}
