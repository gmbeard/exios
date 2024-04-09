#ifndef EXIOS_POLL_WAKE_EVENT_HPP_INCLUDED
#define EXIOS_POLL_WAKE_EVENT_HPP_INCLUDED

#include "exios/file_descriptor.hpp"
#include <cinttypes>

namespace exios
{
struct PollWakeEvent
{
    PollWakeEvent();

    auto trigger(std::uint32_t) const noexcept -> void;
    auto reset() const noexcept -> void;
    auto get_fd() const noexcept -> int;

private:
    FileDescriptor fd_;
};
} // namespace exios

#endif // EXIOS_POLL_WAKE_EVENT_HPP_INCLUDED
