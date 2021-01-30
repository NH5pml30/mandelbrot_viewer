#include "mandelbrot_viewer.h"
#include <QtWidgets/QApplication>
// #include <vld.h>

#include <type_traits>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  mandelbrot_viewer w;
  w.show();
  return a.exec();
}
