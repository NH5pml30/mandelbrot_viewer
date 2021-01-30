#include <fstream>
#include <QEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

#include "mapper_widget.h"

mapper_widget::mapper_widget(QWidget *parent) : QWidget(parent)
{
  ui.setupUi(this);
  connect(&worker, &mapper_enterprise::output_update, this, &mapper_widget::part_image_update);
  connect(&worker, &mapper_enterprise::output_redraw, this, &mapper_widget::full_image_update);
}

mapper_widget::~mapper_widget()
{}

void mapper_widget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
  {
    left_bt_pressed = true;
    last_mouse_pos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
  }
  else
    event->ignore();
}

QPointF mapper_widget::calc_posf(QPoint pos)
{
  return {pos.x() * 1.0 / width(), pos.y() * 1.0 / height()};
}

void mapper_widget::wheelEvent(QWheelEvent *event)
{
  worker.zoom(calc_posf(event->pos()), event->delta() / 120);
  event->accept();
}

void mapper_widget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
  {
    left_bt_pressed = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
  }
  else
    event->ignore();
}

void mapper_widget::mouseMoveEvent(QMouseEvent *event) {
  if (left_bt_pressed)
  {
    QPoint pt = event->pos();
    worker.pan(calc_posf(pt - last_mouse_pos));
    last_mouse_pos = pt;
    event->accept();
  }
  else
    event->ignore();
}

// works faster and more stably than QPainter::drawImage(QRect dst, QImage, QRect src)
void draw_mip(std::vector<pixel_helper::color> &scr_buf, int scr_w, int scr_h,
              pixel_helper::color *data, int scr_x, int scr_y, int mip_w, int mip_h, int mip_level)
{
  int sq_size = 1 << mip_level;

  for (int i = 0; i < mip_h; i++)
    for (int k = 0; k < sq_size; k++)
    {
      int y = scr_y + i * sq_size + k;
      if (y >= 0 && y < scr_h)
        for (int j = 0; j < mip_w; j++)
          for (int l = 0; l < sq_size; l++)
          {
            int x = scr_x + j * sq_size + l;
            if (x >= 0 && x < scr_w)
              scr_buf[y * scr_w + x] = data[i * mip_w + j];
          }
    }
}

void mapper_widget::paintEvent(QPaintEvent *event)
{
  is_update_queued = false;

  while (!update_scr_queue.empty())
  {
    update_scr_query query = std::move(update_scr_queue.front());
    update_scr_queue.pop_front();
    draw_mip(cached_result, width(), height(), query.data.data(), query.scr_x, query.scr_y, query.mip_w, query.mip_h,
             query.mip_level);
  }

  // full_image_update();
  QPainter p(this);
  p.drawImage(event->rect(), QImage((uchar *)cached_result.data(), width(), height(), width() * 3, QImage::Format_RGB888));
}

void mapper_widget::resizeEvent(QResizeEvent *event)
{
  cached_result.resize(event->size().width() * event->size().height());
  worker.resize(event->size());
}

int round_up_mip_level(int x, int mip_size, int mip_level)
{
  return ((x + (mip_size << mip_level) - 1) / (mip_size << mip_level) + 1) * mip_size;
}

void mapper_widget::full_image_update()
{
  update_scr_queue.clear();

  worker.visit_output([&](pixel_helper::color *data, int scr_x, int scr_y, int mip_size, int mip_level) {
    update_scr_query query = update_scr_query(scr_x, scr_y, mip_size, mip_size, mip_level);
    std::copy(data, data + query.data.size(), query.data.begin());
    update_scr_queue.push_back(std::move(query));
  });
  if (!is_update_queued)
  {
    is_update_queued = true;
    QTimer::singleShot(10, this, SLOT(update()));
  }
}

void mapper_widget::part_image_update(pixel_helper::color *data, int scr_x, int scr_y, int mip_size, int mip_level)
{
  update_scr_query query = update_scr_query(scr_x, scr_y, mip_size, mip_size, mip_level);
  std::copy(data, data + query.data.size(), query.data.begin());
  update_scr_queue.push_back(std::move(query));
  if (!is_update_queued)
  {
    is_update_queued = true;
    QTimer::singleShot(10, this, SLOT(update()));
  }
}

void mapper_widget::change_draft_mip_level_event(int new_draft_mip_level)
{
  worker.change_draft_mip_level(new_draft_mip_level);
}
