#pragma once

#include <QWidget>
#include <deque>

#include "ui_mapper_widget.h"
#include "mapper_enterprise.h"

class mapper_widget : public QWidget
{
  Q_OBJECT

public:
  mapper_widget(QWidget *parent = Q_NULLPTR);
  ~mapper_widget();

  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void full_image_update();
  void part_image_update(pixel_helper::color *data, int x, int y, int size, int mip_level);
  void change_draft_mip_level_event(int new_draft_mip_level);

private:
  QPointF calc_posf(QPoint pos);

  bool left_bt_pressed = false;
  QPoint last_mouse_pos;

  mapper_enterprise worker;
  std::vector<pixel_helper::color> cached_result;

  bool is_update_queued = false, is_full_update_queued = false;
  std::chrono::high_resolution_clock::time_point last_update;

  struct update_scr_query
  {
    int scr_x, scr_y, mip_w, mip_h, mip_level;
    std::vector<pixel_helper::color> data;

    update_scr_query(int scr_x = -1, int scr_y = -1, int mip_w = 0, int mip_h = 0, int mip_level = 0) : scr_x(scr_x), scr_y(scr_y), mip_level(mip_level)
    {
      resize(mip_w, mip_h);
    }

    void resize(int new_w, int new_h)
    {
      mip_w = new_w, mip_h = new_h;
      data.resize(new_w * new_h);
    }
  };
  std::deque<update_scr_query> update_scr_queue;

  Ui::mapper_widget ui;
};
