#ifndef EXIOS_WORK_HPP_INCLUDED
#define EXIOS_WORK_HPP_INCLUDED

#include <type_traits>
#include <utility>

namespace exios
{

template <typename T>
concept HasContextAccessor = requires(T val) {
    {
        val.get_context()
    };
};

template <typename T>
struct InnerContext
{
    using type = std::decay_t<T>;
};

template <HasContextAccessor T>
struct InnerContext<T>
{
    using type = std::decay_t<decltype(std::declval<T>().get_context())>;
};

template <typename T>
using InnerContextType = typename InnerContext<T>::type;

template <typename ContextType>
struct Work
{
    explicit Work(ContextType const& ctx) noexcept
    requires(HasContextAccessor<ContextType>)
        : ctx_ { ctx.get_context() }
    {
        ctx_.latch_work();
    }

    explicit Work(ContextType const& ctx) noexcept
    requires(!HasContextAccessor<ContextType>)
        : ctx_ { ctx }
    {
        ctx_.latch_work();
    }

    Work(Work&& other) noexcept
        : ctx_ { other.ctx_ }
        , active_ { std::exchange(other.active_, false) }
    {
    }

    Work(Work const&&) noexcept = delete;
    ~Work() { reset(); }

    auto operator=(Work const&&) noexcept -> Work& = delete;
    auto reset() noexcept -> void
    {
        if (active_)
            ctx_.release_work();
        active_ = false;
    }

private:
    using InnerType = InnerContextType<ContextType>;
    InnerType ctx_;
    bool active_ { true };
};

template <typename F, typename ContextType>
auto wrap_work(F&& f, ContextType const& ctx)
{
    return [work = Work(ctx), f = std::move(f)](auto&&... args) mutable {
        return f(std::forward<decltype(args)>(args)...);
    };
}

} // namespace exios

#endif // EXIOS_WORK_HPP_INCLUDED
