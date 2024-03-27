#include "exios/timer.hpp"
#include <unistd.h>
#include <utility>

namespace exios
{
FileDescriptor::FileDescriptor(int fd) noexcept
    : fd_ { fd }
{
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
    : fd_ { std::exchange(other.fd_, kInvalidDescriptor) }
{
}

FileDescriptor::~FileDescriptor()
{
    if (fd_ >= 0)
        ::close(fd_);
}

auto swap(FileDescriptor& lhs, FileDescriptor& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.fd_, rhs.fd_);
}

auto FileDescriptor::operator=(FileDescriptor&& other) noexcept
    -> FileDescriptor&
{
    auto tmp { std::move(other) };
    swap(*this, tmp);
    return *this;
}

auto FileDescriptor::value() const noexcept -> int const& { return fd_; }

} // namespace exios
