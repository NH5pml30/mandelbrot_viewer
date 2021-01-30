#include <cmath>

#include "camera.h"

bool camera::zoom(QPointF mposf, int delta)
{
  qreal fac = pow(ZOOM_FACTOR, delta);
  if (get_pixel_scale() * fac < MIN_PIXEL_SCALE)
    fac = MIN_PIXEL_SCALE / get_pixel_scale();
  if (fac == 1)
    return false;

  QSizeF size = screen.size();
  screen.moveTopLeft({screen.left() + (1 - fac) * screen.width() * mposf.x(),
                      screen.top() + (1 - fac) * screen.height() * mposf.y()});
  screen.setSize(size * fac);
  return true;
}

void camera::pan(QPointF mdposf)
{
  screen.moveTopLeft(screen.topLeft() +
                     QPointF(-mdposf.x() * screen.width(), -mdposf.y() * screen.height()));
}

void camera::resize(QSize size)
{
  screen.setSize(QSizeF(size) * get_pixel_scale());
  img_size = size;
}

qreal camera::get_pixel_scale() const
{
  return screen.width() / img_size.width();
}

QPointF camera::to_point(int x, int y) const
{
  return screen.topLeft() + QPointF(x, y) * get_pixel_scale();
}

QPoint camera::from_point(QPointF pt) const
{
  QPointF xy = pt - screen.topLeft();
  return {static_cast<int>(xy.x() / get_pixel_scale()),
          static_cast<int>(xy.y() / get_pixel_scale())};
}
