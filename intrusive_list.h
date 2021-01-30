#pragma once

#include <iterator>

namespace intrusive
{
  struct default_tag;

  template <typename Tag = default_tag>
  struct list_element
  {
    list_element *prev = nullptr;
    list_element *next = nullptr;

    void unlink()
    {
      if (next != nullptr)
      {
        next->prev = prev;
      }
      if (prev != nullptr)
      {
        prev->next = next;
      }
      prev = next = nullptr;
    }
  };

  template <typename T, typename Tag = default_tag>
  class list
  {
  private:
    list_element<Tag> head_, tail_;
    list_element<Tag> *head = &head_;
    list_element<Tag> *tail = &tail_;

    template<typename ItT>
    class iterator_ex
    {
      friend class list;
    protected:
      list_element<Tag> *data;
      explicit iterator_ex(list_element<Tag> *data) noexcept : data(data) {}

    public:
      using value_type        = ItT;
      using difference_type   = std::ptrdiff_t;
      using pointer           = ItT *;
      using reference         = ItT &;
      using iterator_category = std::bidirectional_iterator_tag;

      iterator_ex() = default;

      reference operator*() const noexcept  { return static_cast<reference>(*data); }
      pointer operator->() const noexcept { return static_cast<pointer>(data); }

      bool operator==(iterator_ex other) const noexcept { return data == other.data; }
      bool operator!=(iterator_ex other) const noexcept { return data != other.data; }

      iterator_ex& operator--() noexcept
      {
        data = data->prev;
        return *this;
      }
      iterator_ex& operator++() noexcept
      {
        data = data->next;
        return *this;
      }
      iterator_ex operator--(int) noexcept
      {
        iterator_ex res(data);
        operator--();
        return res;
      }
      iterator_ex operator++(int) noexcept
      {
        iterator_ex res(data);
        operator++();
        return res;
      }

      operator iterator_ex<const ItT>() const noexcept
      {
        return iterator_ex<const ItT>(data);
      }
    };

  public:
    using iterator = iterator_ex<T>;
    using const_iterator = iterator_ex<const T>;

    static_assert(std::is_convertible_v<T &, list_element<Tag> &>,
        "value type is not convertible to list_element");

    list() noexcept : head_{ nullptr, &tail_ }, tail_{ &head_, nullptr }
    {
    }
    list(const list &) = delete;
    list(list &&other) noexcept : list()
    {
      splice(end(), other, other.begin(), other.end());
    }
    ~list()
    {
    }

    list &operator=(const list &) = delete;
    list &operator=(list &&other) noexcept
    {
      clear();
      splice(end(), other, other.begin(), other.end());
      return *this;
    }

    void clear() noexcept
    {
      auto at = head->next;
      while (at != tail)
      {
        auto save = at->next;
        at->prev = at->next = nullptr;
        at = save;
      }
      head->next = tail;
      tail->prev = head;
    }

    void push_back(T &el) noexcept
    {
      insert(end(), el);
    }
    void pop_back() noexcept
    {
      back().list_element<Tag>::unlink();
    }
    T &back() noexcept
    {
      return *--end();
    }
    const T &back() const noexcept
    {
      return *--end();
    }

    void push_front(T& el) noexcept
    {
      insert(begin(), el);
    }
    void pop_front() noexcept
    {
      front().list_element<Tag>::unlink();
    }
    T &front() noexcept
    {
      return *begin();
    }
    const T &front() const noexcept
    {
      return *begin();
    }

    bool empty() const noexcept
    {
      return head->next == tail;
    }

    iterator begin() noexcept
    {
      return iterator(head->next);
    }
    const_iterator begin() const noexcept
    {
      return const_iterator(head->next);
    }

    iterator end() noexcept
    {
      return iterator(tail);
    }
    const_iterator end() const noexcept
    {
      return const_iterator(tail);
    }

    iterator insert(const_iterator pos, T &el) noexcept
    {
      list_element<Tag> *at = pos.data;
      el.list_element<Tag>::next = at;
      el.list_element<Tag>::prev = at->prev;
      at->prev->next = &el;
      at->prev = &el;
      return iterator(&el);
    }
    iterator erase(const_iterator pos) noexcept
    {
      list_element<Tag> *at = pos.data;
      auto saved = at->next;
      at->unlink();
      return iterator(saved);
    }
    void splice(const_iterator pos, list &other, const_iterator first, const_iterator last) noexcept
    {
      list_element<Tag> *at = pos.data;
      list_element<Tag> *begin = first.data;
      list_element<Tag> *end = last.data;

      if (begin == end)
      {
        return;
      }
      end = end->prev;

      begin->prev->next = end->next;
      end->next->prev = begin->prev;

      begin->prev = at->prev;
      end->next = at;
      at->prev->next = begin;
      at->prev = end;
    }

    iterator iterator_from_el(T &el)
    {
      return iterator(&el);
    }
    const_iterator iterator_from_el(const T &el) const
    {
      return const_iterator(&el);
    }
  };
}
