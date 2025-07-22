#ifndef UNIQUE_RESOURCE_HPP
#define UNIQUE_RESOURCE_HPP

#include <concepts>
#include <utility>

// TODO: requirements on T and D
template <typename T, typename D>
class Unique_resource
{
public:
    // TODO: generalized "invalid handle" value other than relying on operator
    // bool (via a trait on T for example)

    // TODO: noexcept based on type traits
    constexpr Unique_resource() : m_handle {}, m_deleter {}
    {
    }

    // TODO: noexcept based on type traits
    template <typename UT, typename UD = D>
        requires std::constructible_from<T, UT &&> &&
                     std::constructible_from<D, UD &&> &&
                     (!std::same_as<std::remove_cvref_t<UT>, Unique_resource>)
    explicit constexpr Unique_resource(UT &&handle, UD &&deleter = UD())
        : m_handle {std::forward<UT>(handle)},
          m_deleter {std::forward<UD>(deleter)}
    {
    }

    constexpr Unique_resource(Unique_resource &&rhs) noexcept
        : m_handle {std::move(rhs.m_handle)},
          m_deleter {std::move(rhs.m_deleter)}
    {
        rhs.m_handle = T();
    }

    // TODO: reset/release
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

    [[nodiscard]] constexpr const T &get() const noexcept
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

template <class UT, class UD>
Unique_resource(UT &&, UD &&)
    -> Unique_resource<std::remove_cvref_t<UT>, std::remove_cvref_t<UD>>;

#endif
