#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_mandelbrot_viewer.h"
#include "mapper_widget.h"
#include "mandelbrot_settings_dialog.h"

class mandelbrot_viewer : public QMainWindow
{
  Q_OBJECT

public:
  mandelbrot_viewer(QWidget *parent = Q_NULLPTR);

public slots:
  void on_settings();
  void on_draftLevelChanged(int new_draft_level);

private:

  Ui::mandelbrot_viewerClass ui;
  mandelbrot_settings_dialog dlg;
  mapper_widget widget;
};
