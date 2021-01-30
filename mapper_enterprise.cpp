#include <complex>

#include "mapper_enterprise.h"

static int calc_mandelbrot(std::complex<qreal> Z)
{
  int n;
  std::complex<qreal> Z0 = Z;
  for (n = 0; n < 255 && std::norm(Z) < 4; n++, Z = Z * Z + Z0)
    ;
  return n;
}

static pixel_helper::color float2color(qreal X)
{
  using color = pixel_helper::color;
  qreal c = X;
  c = c * (2 - c);
  c = fmod(c * 7 * 3, 7);

  switch (static_cast<int>(c))
  {
  case 0:
    return color(c, 0, 0);
  case 1:
    return color(1, c - 1, 0);
  case 2:
    return color(3 - c, 1, 0);
  case 3:
    return color(0, 1, c - 3);
  case 4:
    return color(0, 5 - c, 1);
  case 5:
    return color(c - 5, 0, 1);
  default:
    return color(1, c - 6, 1);
  }
}

pixel_helper::color mapper_enterprise::pixel_getter(QPointF pt)
{
  return float2color(calc_mandelbrot({pt.x(), pt.y()}) / 255.0);
}

mapper_enterprise::superpixel_base::superpixel_base(const superpixel_base &other) noexcept
    : base_t(other), input_version(other.input_version.load()), is_draft(other.is_draft)
{
}

mapper_enterprise::superpixel_base &mapper_enterprise::superpixel_base::operator=(const superpixel_base &other) noexcept
{
  if (&other != this)
  {
    base_t::operator=(other);
    input_version.store(other.input_version);
    is_draft = other.is_draft;
  }
  return *this;
}

mapper_enterprise::mapper_enterprise()
{
  std::lock_guard lglg(lg);

  allocated_pixels.push_back(std::make_unique<superpixel[]>(pool_size));
  for (size_t i = 0; i < pool_size; i++)
    pixel_pool.push_back(allocated_pixels.back()[i]);

  for (int i = 0; i < N_WORKERS; i++)
    workers.emplace_back(std::thread([this, i] {
      uint64_t last_input_version = 0;
      for (;;)
      {
        superpixel_base copy;
        superpixel *result_spot;
        {
          std::unique_lock lg(m);
          result_spot = &task_queue.pop(lg);
          copy = *result_spot;
        }

        if (copy.input_version == superpixel::INPUT_VERSION::QUIT)
          break;

        render_superpixel(copy, *result_spot);
      }
    }));
}

mapper_enterprise::~mapper_enterprise()
{
  std::vector<superpixel> quit(N_WORKERS);
  {
    std::lock_guard lglg(lg);
    for (auto &row : screen)
      free_superpixel_row(row);
    screen.clear();
    for (int i = 0; i < N_WORKERS; i++)
    {
      quit[i].input_version = superpixel::INPUT_VERSION::QUIT;
      quit[i].set_mip_level(draft_mip_level);
      quit[i].is_draft = true;
      task_queue.push(quit[i]);
    }
  }

  for (auto &th : workers)
    th.join();
}

// pre: global mutex is locked
QPoint mapper_enterprise::superpixel2screen(superpixel &p)
{
  QPointF superpixel_ul_corner = screen.front().front().ul_corner;
  QPoint screen_coord0 = cam.from_point(superpixel_ul_corner), screen_coord = screen_coord0;

  for (auto &row : screen)
  {
    for (auto &sq : row)
    {
      if (&sq == &p)
        return screen_coord;
      screen_coord.setX(screen_coord.x() + superpixel_size);
    }
    screen_coord.setX(screen_coord0.x());
    screen_coord.setY(screen_coord.y() + superpixel_size);
  }

  return screen_coord;
}

void mapper_enterprise::notify_output(mapper_enterprise::superpixel *p, size_t input_version)
{
  bool unlock = false;
  if (!lg.owns_lock())
  {
    // This is the only method that can be called from event queue:
    // sometimes can reenter here through system call from another method => lg can be locked.
    // Otherwise need to lock
    lg.lock();
    unlock = true;
  }
  if (p->input_version == input_version && !p->is_draft)
  {
    QPoint coords = superpixel2screen(*p);
    emit output_update(p->get_mip_data(), coords.x(), coords.y(), superpixel_size >> p->last_mip_level,
                       p->last_mip_level);
  }
  if (unlock)
    lg.unlock();
}

// pre: global mutex is locked
mapper_enterprise::superpixel &mapper_enterprise::allocate_superpixel(QPointF ul_corner, qreal scale)
{
  if (pixel_pool.empty())
  {
    allocated_pixels.push_back(std::make_unique<superpixel[]>(pool_size));
    for (size_t i = 0; i < pool_size; i++)
      pixel_pool.push_back(allocated_pixels.back()[i]);
    pool_size *= 2;
  }

  superpixel &p = pixel_pool.front();
  pixel_pool.pop_front();
  p.ul_corner = ul_corner;
  p.scale = scale;
  p.input_version = input_version;
  p.set_mip_level(draft_mip_level);
  p.is_draft = true;
  task_queue.push(p);
  added_pixels++;
  return p;
}

// pre: global mutex is locked
void mapper_enterprise::free_superpixel(mapper_enterprise::superpixel &pixel)
{
  pixel.input_version = input_version;
  task_queue.erase(pixel);
  pixel.clear();
  pixel_pool.push_back(pixel);
}

// pre: global mutex is locked
void mapper_enterprise::free_superpixel_row(intrusive::list<mapper_enterprise::superpixel, screen_tag> &row)
{
  for (auto &sq : row)
    free_superpixel(sq);
  row.clear();
}

// pre: global mutex is locked (unlocks)
void mapper_enterprise::update_screen()
{
  update_screen_func<false>();
  added_pixels = 0;
  update_screen_func<true>();
  // pull all resources to draft
  if (added_pixels > 0)
    for (auto &row : screen)
      for (auto &sq : row)
        if (!sq.is_tasked && sq.last_mip_level != 0)
        {
          // sq is being rendered right now, push it off
          sq.input_version = input_version;
          task_queue.push(sq);
        }
  ++input_version;

  auto begin = std::chrono::high_resolution_clock::now();
  rendered_drafts.wait(lg, added_pixels);
  lg.unlock();
  auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
  if (dt > 20)
    my_log::println("Update took " + std::to_string(dt) + "ms");
  emit output_redraw();
}

void mapper_enterprise::pan(QPointF mdposf)
{
  auto begin = std::chrono::high_resolution_clock::now();
  cam.pan(mdposf);
  {
    lg.lock();
    update_screen();
  }
  auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
  if (dt > 20)
    my_log::println("Pan took " + std::to_string(dt) + "ms");
}

void mapper_enterprise::zoom(QPointF mposf, int delta)
{
  if (!cam.zoom(mposf, delta))
    return;
  superpixel_scale = superpixel_size * cam.get_pixel_scale();
  {
    lg.lock();
    for (auto &row : screen)
      free_superpixel_row(row);
    screen.clear();
    update_screen();
  }
}

void mapper_enterprise::resize(QSize size)
{
  cam.resize(size);
  {
    lg.lock();
    update_screen();
  }
}

void mapper_enterprise::change_draft_mip_level(int new_draft_mip_level)
{
  std::lock_guard lglg(lg);
  draft_mip_level = new_draft_mip_level;
}

void mapper_enterprise::render_superpixel(mapper_enterprise::superpixel_base &pixel, mapper_enterprise::superpixel &result_spot)
{
  if (!pixel.render_mip_level([&] { return pixel.input_version != result_spot.input_version; }))
    return;
  {
    std::unique_lock lg(m);
    if (pixel.input_version != result_spot.input_version)
      return;
    assert(pixel.last_mip_level >= -1);
    result_spot.copy_mip_data(pixel);
    result_spot.is_draft = false;
    if (pixel.last_mip_level != 0)
      task_queue.push(result_spot);  // can rerender with higher quality
    if (!pixel.is_draft)
    {
      size_t input_version = result_spot.input_version;
      mapper_enterprise::superpixel *spot = &result_spot;
      // screen updated
      QMetaObject::invokeMethod(this, "notify_output", Qt::QueuedConnection,
                                Q_ARG(mapper_enterprise::superpixel *, spot),
                                Q_ARG(size_t, input_version));
    }
    else
      // rendered draft pixel
      rendered_drafts.increment(lg);
  }
}
