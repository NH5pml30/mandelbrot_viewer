#pragma once

#include <ratio>
#include <QRect>

struct camera
{
  QRectF screen = QRectF(-2., -2., 4., 4.);
  QSize img_size = QSize(screen.width() * 100, screen.height() * 100);

  bool zoom(QPointF mposf, int delta);
  void pan(QPointF mdposf);
  void resize(QSize size);

  qreal get_pixel_scale() const;

  QPointF to_point(int x, int y) const;
  QPoint from_point(QPointF pt) const;

  template<class ratio = std::ratio<1, 1>>
  bool intersects_x(qreal pt_l, qreal pt_r) const
  {
    qreal delta = (ratio::num * 1.0 / ratio::den - 1) * 0.5 * screen.width();
    return pt_r >= screen.left() - delta && pt_l <= screen.right() + delta;
  }

  template<class ratio = std::ratio<1, 1>>
  bool intersects_y(qreal pt_t, qreal pt_b) const
  {
    qreal delta = (ratio::num * 1.0 / ratio::den - 1) * 0.5 * screen.height();
    return pt_b >= screen.top() - delta && pt_t <= screen.bottom() + delta;
  }

  static constexpr qreal MIN_PIXEL_SCALE = 1e-16;
  static constexpr qreal ZOOM_FACTOR = 0.8;
};
