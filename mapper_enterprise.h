#pragma once
#include <thread>
#include <condition_variable>
#include <list>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>

#include <QImage>
#include <QRect>

#include "intrusive_list.h"
#include "camera.h"
#include "superpixel.h"
#include "task_queue.h"

namespace my_log
{
  inline constexpr bool IS_LOG = false;

  inline void println(const std::string &str)
  {
    if constexpr (IS_LOG)
    {
      static std::ofstream file_log = std::ofstream("myplog.txt");
      static std::mutex m;

      std::stringstream ss;
      ss << std::this_thread::get_id();
      auto now = std::chrono::system_clock::now();
      auto s0 = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
      auto ms0 = std::chrono::duration_cast<std::chrono::milliseconds>(s0);
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
      auto dms = ms - ms0;
      std::time_t time = std::chrono::system_clock::to_time_t(now);
      std::lock_guard lg(m);
      char mbstr[100]{};
      std::strftime(mbstr, sizeof(mbstr), "%T", std::localtime(&time));
      ss << " (" << mbstr << ":" << dms.count() << ", " << clock() << "): " << str << std::endl;

      file_log << ss.rdbuf();
    }
  }
}  // namespace my_log

class mapper_enterprise : public QObject
{
  Q_OBJECT

public:
  mapper_enterprise();
  ~mapper_enterprise();

  /* Modify input functions */
  void zoom(QPointF mposf, int delta);
  void pan(QPointF mdposf);
  void resize(QSize size);
  void change_draft_mip_level(int new_draft_mip_level);

  /* Get rendered screen function */
  template<class Func>
  void visit_output(Func &&func);

signals:
  /* Signals on rendered screen changed */
  void output_update(pixel_helper::color *data, int x, int y, int size, int mip_level);
  void output_redraw();

private:
  static pixel_helper::color pixel_getter(QPointF pt);
  static constexpr size_t superpixel_size_pow = 8;
  static constexpr size_t superpixel_size = 1 << superpixel_size_pow;

  // type for task queue
  struct superpixel_base : intrusive::list_element<struct task_pool_tag>,
                           intrusive::list_element<struct screen_tag>,
                           superpixel_f<pixel_getter, superpixel_size>
  {
    using base_t = superpixel_f<pixel_getter, superpixel_size>;
    using base_t::superpixel_f;

    superpixel_base(const superpixel_base &other) noexcept;
    superpixel_base &operator=(const superpixel_base &other) noexcept;

    size_t priority()
    {
      return last_mip_level - 1;
    }

    enum INPUT_VERSION : size_t
    {
      QUIT, NORMAL
    };
    std::atomic<size_t> input_version;
    bool is_draft;
  };

public:
  static constexpr int max_draft_mip_level = superpixel_size_pow;

private:
  int draft_mip_level = max_draft_mip_level;
  using task_queue_t = task_queue_ex<superpixel_base, task_pool_tag, max_draft_mip_level>;
public:
  // public for qt meta argument
  using superpixel = typename task_queue_t::store_type;

private slots:
  void notify_output(mapper_enterprise::superpixel *p, size_t input_version);

private:
  /* Modify superpixels functions */
  // pre: global mutex is locked
  superpixel &allocate_superpixel(QPointF ul_corner, qreal scale);
  // pre: global mutex is locked
  void free_superpixel(superpixel &pixel);
  // pre: global mutex is locked
  void free_superpixel_row(intrusive::list<superpixel, screen_tag> &row);

  /* Update screen's superpixels functions */
  // pre: global mutex is locked
  template<bool (camera::*Intersect)(qreal, qreal) const, typename T, class TFactory, class TCollector>
  void clear_screen_dim(T &obj, qreal corner, TFactory &&factory, TCollector &&collector);

  // pre: global mutex is locked
  template<bool (camera::*Intersect)(qreal, qreal) const, typename T, class TFactory, class TCollector>
  void build_screen_dim(T &obj, qreal corner, TFactory &&factory, TCollector &&collector);

  template<bool is_building, bool (camera::*Intersect)(qreal, qreal) const, typename T, class Tf, class Tc>
  static constexpr auto update_screen_dim_func_ex =
      is_building ? &mapper_enterprise::build_screen_dim<Intersect, T, Tf, Tc>
                  : &mapper_enterprise::clear_screen_dim<Intersect, T, Tf, Tc>;

  // pre: global mutex is locked
  template<bool is_building>
  void update_screen_func();

  // pre: global mutex is locked (unlocks)
  void update_screen();

  /* Render superpixel */
  void render_superpixel(superpixel_base &pixel, superpixel &result_spot);

  /* Other utils */
  // pre: global mutex is locked
  QPoint superpixel2screen(superpixel &p);

  /* Input version */
  size_t input_version = superpixel::INPUT_VERSION::NORMAL;

  /* Location in space data */
  camera cam;
  qreal superpixel_scale = superpixel_size * cam.get_pixel_scale();

  /* Workers & superpixels storage */
  std::vector<std::thread> workers;
  intrusive::list<superpixel, task_pool_tag> pixel_pool;
  size_t pool_size = 1;
  std::list<intrusive::list<superpixel, screen_tag>> screen;
  std::vector<std::unique_ptr<superpixel[]>> allocated_pixels;
  task_queue_t task_queue;

  WAITING_COUNTER rendered_drafts;  // number of rendered drafts on current input change
  size_t added_pixels = 0;          // number of added pixels on current input change
  mutable std::mutex m;             // global lock
  mutable std::unique_lock<std::mutex> lg = std::unique_lock(m, std::defer_lock);

  // number of multithreaded workers
  const unsigned N_WORKERS = std::thread::hardware_concurrency();
};

Q_DECLARE_METATYPE(mapper_enterprise::superpixel *)
Q_DECLARE_METATYPE(size_t)

template<bool (camera::*Intersect)(qreal, qreal) const, typename T, class TFactory, class TCollector>
void mapper_enterprise::clear_screen_dim(T &obj, qreal corner, TFactory &&factory, TCollector &&collector)
{
  for (auto el = obj.begin(); el != obj.end(); corner += superpixel_scale)
  {
    if (!(cam.*Intersect)(corner, corner + superpixel_scale))
    {
      collector(*el);
      el = obj.erase(el);
    }
    else
      ++el;
  }
}

template<bool (camera::*Intersect)(qreal, qreal) const, typename T, class TFactory, class TCollector>
void mapper_enterprise::build_screen_dim(T &obj, qreal corner, TFactory &&factory, TCollector &&collector)
{
  using lim = std::numeric_limits<qreal>;
  // Here, infinities are used to render all superpixels without leaving holes,
  // because physical screen can jump over cached superpixels border on caching screen:
  //       caching screen
  //    +---------------------
  //    |
  // old|screen    new screen
  //   -| -- -+    +----------
  //    |     |    |
  //    |****      |
  //    |**** |    |
  //    |****      |
  //    |     |    |
  //    |          |
  //    |     |    |
  //    |          |
  //   -| -- -+    +----------
  //    |        ^hole
  //    +---------------------
  auto saved_begin = obj.begin();
  for (qreal corner_back = corner - superpixel_scale;
        (cam.*Intersect)(-lim::infinity(), corner_back + superpixel_scale); corner_back -= superpixel_scale)
    obj.push_front(factory(corner_back));
  for (auto el = saved_begin; el != obj.end(); ++el, corner += superpixel_scale)
    ;
  for (; (cam.*Intersect)(corner, lim::infinity()); corner += superpixel_scale)
    obj.push_back(factory(corner));
}

template<bool is_building>
void mapper_enterprise::update_screen_func()
{
  QPointF ul_corner = screen.empty() ? cam.screen.topLeft() : 
    screen.front().empty() ? cam.screen.topLeft() : screen.front().front().ul_corner;
  qreal corner_y;

  auto factory_y = [&](qreal corner_y) {
    ul_corner.setY(std::min(ul_corner.y(), corner_y));
    return intrusive::list<superpixel, screen_tag>();
  };
  auto collector_y = [this](intrusive::list<superpixel, screen_tag> &row) {
    free_superpixel_row(row);
  };
  auto factory_x = [&](qreal corner_x) -> superpixel & {
    superpixel &res = allocate_superpixel({corner_x, corner_y}, superpixel_scale);
    return res;
  };
  auto collector_x = [this](superpixel &pixel) { free_superpixel(pixel); };

  // cache some of the near superpixels
  using screen_ratio = std::conditional_t<is_building, std::ratio<1, 1>, std::ratio<3, 2>>;
  static constexpr auto update_func_x =
      update_screen_dim_func_ex<is_building, &camera::intersects_x<screen_ratio>, decltype(*screen.begin()),
                                decltype(factory_x) &, decltype(collector_x) &>;
  static constexpr auto update_func_y =
      update_screen_dim_func_ex<is_building, &camera::intersects_y<screen_ratio>, decltype(screen),
                                decltype(factory_y) &, decltype(collector_y) &>;

  (this->*update_func_y)(screen, ul_corner.y(), factory_y, collector_y);
  corner_y = ul_corner.y();
  for (auto row = screen.begin(); row != screen.end(); ++row, corner_y += superpixel_scale)
    (this->*update_func_x)(*row, ul_corner.x(), factory_x, collector_x);
}

template<class Func>
inline void mapper_enterprise::visit_output(Func &&func)
{
  QPointF superpixel_ul_corner = screen.front().front().ul_corner;
  QPoint screen_coord0 = cam.from_point(superpixel_ul_corner), screen_coord = screen_coord0;

  // cut out cached screen and show only physical
  std::lock_guard lglg(lg);
  for (auto &row : screen)
  {
    if (!row.empty() && cam.intersects_y(row.front().ul_corner.y(), row.front().ul_corner.y() + superpixel_scale))
    {
      for (auto &sq : row)
      {
        if (cam.intersects_x(sq.ul_corner.x(), sq.ul_corner.x() + superpixel_scale))
          func(sq.get_mip_data(), screen_coord.x(), screen_coord.y(), superpixel_size >> sq.last_mip_level,
               sq.last_mip_level);
        screen_coord.setX(screen_coord.x() + superpixel_size);
      }
    }
    screen_coord.setX(screen_coord0.x());
    screen_coord.setY(screen_coord.y() + superpixel_size);
  }
}
