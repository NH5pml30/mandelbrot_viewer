#include "mandelbrot_settings_dialog.h"
#include "mapper_enterprise.h"

mandelbrot_settings_dialog::mandelbrot_settings_dialog(QWidget *parent)
    : QDialog(parent), settings("NH5 Software", "Mandelbrot Viewer")
{
  ui.setupUi(this);
  int level = settings.value("Draft level", 4).toInt();
  ui.spinBox->setMinimum(0);
  ui.spinBox->setMaximum(mapper_enterprise::max_draft_mip_level);
  ui.spinBox->setValue(level);
}

int mandelbrot_settings_dialog::get_draft_level() const
{
  return ui.spinBox->value();
}

void mandelbrot_settings_dialog::on_draftLevelChanged(int level)
{
  settings.setValue("Draft level", level);
  emit draft_level_changed(level);
}

mandelbrot_settings_dialog::~mandelbrot_settings_dialog()
{
}
