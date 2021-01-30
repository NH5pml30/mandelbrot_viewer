#pragma once

#include <array>
#include <QTypeInfo>
#include <QPoint>

namespace pixel_helper
{
  struct color
  {
    union
    {
      struct
      {
        uchar r, g, b;
      };
      uchar data[3];
    };

    color() = default;
    color(uchar r, uchar g, uchar b) noexcept : r(r), g(g), b(b) {}
    color(qreal r, qreal g, qreal b) noexcept : r(r * 255), g(g * 255), b(b * 255) {}

    operator uchar *() noexcept
    {
      return data;
    }
    operator const uchar *() const noexcept
    {
      return data;
    }
  };
}  // namespace pixel_helper

template<class PixelColorGetter, size_t Size>
class superpixel
{
public:
  static_assert((Size & (Size - 1)) == 0 && Size != 0, "Size must be a power of 2");
  static constexpr size_t size = Size;
  static constexpr size_t cols_per_line(int mip_level)
  {
    return size >> mip_level;
  }

  superpixel(PixelColorGetter func = {}, QPointF ul_corner = {-2., -2.}, qreal scale = 4.)
      : ul_corner(ul_corner), scale(scale), func(std::move(func))
  {
  }

  void set_mip_level(int mip_level)
  {
    last_mip_level = mip_level + 1;
  }

  template<class LineCallback>
  bool render_mip_level(LineCallback &&callback) const
  {
    --last_mip_level;

    pixel_helper::color *data = get_mip_data();

    size_t off = 0;
    for (size_t y = 0, off = 0; y < cols_per_line(last_mip_level); y++)
    {
      for (size_t x = 0; x < cols_per_line(last_mip_level); x++)
        data[off++] = func(ul_corner + QPointF((x + 0.5) * 1.0 / cols_per_line(last_mip_level),
                                               (y + 0.5) * 1.0 / cols_per_line(last_mip_level)) *
                                           scale);
      if (callback())
        return false;
    }
    return true;
  }

  pixel_helper::color *get_mip_data() const
  {
    assert(last_mip_level != -1);

    size_t offset = 0, s = size * size;
    for (int i = 0; i < last_mip_level; i++, s /= 4)
      offset += s;
    return mip_data.data() + offset;
  }

  void clear()
  {
    last_mip_level = -1;
  }

  void copy_mip_data(superpixel &other)
  {
    mip_data = other.mip_data;
    last_mip_level = other.last_mip_level;
  }

  QPointF ul_corner;
  qreal scale;
  mutable int last_mip_level = -1;

private:
  PixelColorGetter func;
  mutable std::array<pixel_helper::color, size * size * 2> mip_data;
};

template<pixel_helper::color (*Func)(QPointF), size_t Size>
class superpixel_f : public superpixel<pixel_helper::color (*)(QPointF), Size>
{
public:
  superpixel_f() : superpixel<pixel_helper::color (*)(QPointF), Size>(Func) {}
};
