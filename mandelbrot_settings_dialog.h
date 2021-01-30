#pragma once

#include <QDialog>
#include <QSettings>
#include "ui_mandelbrot_settings_dialog.h"

class mandelbrot_settings_dialog : public QDialog
{
  Q_OBJECT

public:
  mandelbrot_settings_dialog(QWidget *parent = Q_NULLPTR);
  ~mandelbrot_settings_dialog();

public:
  int get_draft_level() const;

public slots:
  void on_draftLevelChanged(int level);
signals:
  void draft_level_changed(int level);
private:
  Ui::mandelbrot_settings_dialog ui;
  QSettings settings;
};
