#ifndef UNIQUE_RESOURCE_HPP
#define UNIQUE_RESOURCE_HPP

#include <utility>

// FIXME: proper move and reference semantics
template <typename T, typename D>
class Unique_resource
{
public:
    constexpr Unique_resource() : m_handle {}, m_deleter {}
    {
    }

    constexpr Unique_resource(T object, D &&deleter)
        : m_handle {object}, m_deleter {std::move(deleter)}
    {
    }

    constexpr Unique_resource(T object, const D &deleter = D())
        : m_handle {object}, m_deleter {deleter}
    {
    }

    constexpr Unique_resource(Unique_resource &&rhs) noexcept
        : m_handle {std::move(rhs.m_handle)},
          m_deleter {std::move(rhs.m_deleter)}
    {
        rhs.m_handle = T();
    }

    constexpr Unique_resource &operator=(Unique_resource &&rhs) noexcept
    {
        Unique_resource tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    Unique_resource(const Unique_resource &) = delete;
    Unique_resource &operator=(const Unique_resource &) = delete;

    constexpr ~Unique_resource() noexcept
    {
        if (m_handle)
        {
            m_deleter(m_handle);
        }
    }

    [[nodiscard]] constexpr T get() const noexcept
    {
        return m_handle;
    }

    constexpr void swap(Unique_resource &rhs) noexcept
    {
        using std::swap;
        swap(m_handle, rhs.m_handle);
        swap(m_deleter, rhs.m_deleter);
    }

private:
    T m_handle;
    D m_deleter;
};

#endif
