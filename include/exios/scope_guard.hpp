#ifndef EXIOS_UTILS_SCOPE_GUARD_HPP_INCLUDED
#define EXIOS_UTILS_SCOPE_GUARD_HPP_INCLUDED

#include <utility>

#define EXIOS_GEN_VAR_NAME_IMPL(pfx, line) pfx##line
#define EXIOS_GEN_VAR_NAME(pfx, line) EXIOS_GEN_VAR_NAME_IMPL(pfx, line)
#define EXIOS_SCOPE_GUARD(fn)                                                  \
    auto EXIOS_GEN_VAR_NAME(scope_guard_, __LINE__) = ::exios::ScopeGuard { fn }

namespace exios
{

template <typename F>
struct ScopeGuard
{
    explicit ScopeGuard(F&& f) noexcept
        : fn { std::forward<F>(f) }
    {
    }

    ~ScopeGuard()
    {
        if (active)
            fn();
    }

    ScopeGuard(ScopeGuard const&) = delete;
    auto operator=(ScopeGuard const&) -> ScopeGuard& = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    auto operator=(ScopeGuard&&) -> ScopeGuard& = delete;
    auto deactivate() noexcept -> void { active = false; }

    F fn;
    bool active { true };
};

} // namespace exios

#endif // EXIOS_UTILS_SCOPE_GUARD_HPP_INCLUDED
