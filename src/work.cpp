#include "exios/work.hpp"
#include <utility>

namespace exios
{
Work::Work(Context const& ctx) noexcept
    : ctx_ { ctx }
    , active_ { true }
{
    ctx_.latch_work();
}

Work::Work(Work&& other) noexcept
    : ctx_ { other.ctx_ }
    , active_ { std::exchange(other.active_, false) }
{
}

Work::~Work()
{
    if (active_)
        ctx_.release_work();
}

} // namespace exios
