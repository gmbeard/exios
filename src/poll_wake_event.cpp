#include "exios/poll_wake_event.hpp"
#include "exios/contracts.hpp"
#include <cerrno>
#include <cinttypes>
#include <errno.h>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>

namespace exios
{
PollWakeEvent::PollWakeEvent()
    : fd_ { ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK /*| EFD_SEMAPHORE*/) }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

auto PollWakeEvent::trigger(std::uint32_t n) const noexcept -> void
{
    std::uint64_t val { n };
    [[maybe_unused]] auto const r = ::write(fd_.value(), &val, sizeof(val));
    EXIOS_EXPECT(r >= 0 || errno == EWOULDBLOCK || errno == EAGAIN);
}

auto PollWakeEvent::reset() const noexcept -> void
{
    [[maybe_unused]] std::uint64_t val;
    [[maybe_unused]] auto const r = ::read(fd_.value(), &val, sizeof(val));
    EXIOS_EXPECT(r >= 0 || errno == EWOULDBLOCK || errno == EAGAIN);
}

auto PollWakeEvent::get_fd() const noexcept -> int { return fd_.value(); }
} // namespace exios
