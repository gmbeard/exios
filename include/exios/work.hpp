#ifndef EXIOS_WORK_HPP_INCLUDED
#define EXIOS_WORK_HPP_INCLUDED

#include "exios/context.hpp"

namespace exios
{
struct Work
{
    Work(Context const& ctx) noexcept;
    Work(Work&&) noexcept;
    Work(Work const&&) noexcept = delete;
    ~Work();

    auto operator=(Work const&&) noexcept -> Work& = delete;

private:
    Context ctx_;
    bool active_ { true };
};

template <typename F>
auto wrap_work(F&& f, Context const& ctx)
{
    return [work = Work(ctx), f = std::move(f)](auto&&... args) mutable {
        return f(std::forward<decltype(args)>(args)...);
    };
}

} // namespace exios

#endif // EXIOS_WORK_HPP_INCLUDED
