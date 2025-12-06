#ifndef EXIOS_MESSAGE_HPP_INCLUDED
#define EXIOS_MESSAGE_HPP_INCLUDED

#include "buffer_view.hpp"
#include <algorithm>
#include <cstdint>
#include <sys/socket.h>
#include <type_traits>

namespace exios
{

namespace direction
{
// clang-format off
inline constexpr struct Outgoing { } outgoing {};
inline constexpr struct Incoming { } incoming {};
// clang-format on
} // namespace direction

template <typename Dir>
requires(std::is_same_v<Dir, direction::Outgoing> ||
         std::is_same_v<Dir, direction::Incoming>)
using buffer_view_type_for_direction =
    std::conditional_t<std::is_same_v<Dir, direction::Outgoing>,
                       ConstBufferView,
                       BufferView>;

template <typename Dir, typename Allocator>
requires(std::is_same_v<Dir, direction::Outgoing> ||
         std::is_same_v<Dir, direction::Incoming>)
struct message_base
{
    using allocator =
        std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

    static constexpr bool is_outgoing =
        std::is_same_v<Dir, direction::Outgoing>;

    using buffer_view_type = buffer_view_type_for_direction<Dir>;

    template <typename UAllocator>
    explicit message_base(UAllocator const& alloc) noexcept
        : control_buffer_internal_ { allocator(alloc) }
    {
    }

    message_base() noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {})
    {
    }

    template <typename UAllocator>
    message_base(UAllocator const& alloc,
                 buffer_view_type data_buffer,
                 buffer_view_type control_buffer) noexcept
        : control_buffer_internal_ { allocator(alloc) }
        , control_buffer_ { control_buffer }
        , data_buffer_ { data_buffer }
    {
    }

    template <typename UAllocator>
    message_base(UAllocator const& alloc, buffer_view_type data_buffer) noexcept
        : control_buffer_internal_ { allocator(alloc) }
        , control_buffer_ {}
        , data_buffer_ { data_buffer }
    {
    }

    message_base(buffer_view_type data_buffer) noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {}, data_buffer, buffer_view_type {})
    {
    }

    message_base(buffer_view_type data_buffer,
                 buffer_view_type control_buffer) noexcept
    requires(std::is_default_constructible_v<allocator>)
        : message_base(allocator {}, data_buffer, control_buffer)
    {
    }

    auto set_control_buffer(buffer_view_type buffer) -> message_base&
    {
        control_buffer_ = buffer;
        if (control_buffer_.size) {
            control_buffer_internal_.resize(CMSG_SPACE(control_buffer().size));
        }

        return *this;
    }

    auto set_data_buffer(buffer_view_type buffer) noexcept -> message_base&
    {
        data_buffer_ = buffer;
        return *this;
    }

    [[nodiscard]] auto control_buffer() const noexcept -> buffer_view_type
    {
        return control_buffer_;
    }

    [[nodiscard]] auto data_buffer() const noexcept -> buffer_view_type
    {
        return data_buffer_;
    }

    [[nodiscard]] friend auto message_view(message_base& msg) -> msghdr
    {
        auto result = msghdr {};

        if (msg.control_buffer_.size) {
            msg.control_buffer_internal_.resize(
                CMSG_SPACE(msg.control_buffer().size));

            result.msg_control = msg.control_buffer_internal_.data();
            result.msg_controllen = msg.control_buffer_internal_.size();

            if constexpr (is_outgoing) {
                auto* hdr = CMSG_FIRSTHDR(&result);
                hdr->cmsg_level = SOL_SOCKET;
                hdr->cmsg_type = SCM_RIGHTS;
                hdr->cmsg_len = CMSG_LEN(msg.control_buffer().size);

                std::copy_n(reinterpret_cast<std::uint8_t const*>(
                                msg.control_buffer().data),
                            msg.control_buffer().size,
                            reinterpret_cast<std::uint8_t*>(CMSG_DATA(hdr)));
            }
        }

        return result;
    }

private:
    std::vector<std::uint8_t, allocator> control_buffer_internal_;
    buffer_view_type control_buffer_ {};
    buffer_view_type data_buffer_ {};
};

template <typename Allocator>
using incoming_message_base = message_base<direction::Incoming, Allocator>;

template <typename Allocator>
using outgoing_message_base = message_base<direction::Outgoing, Allocator>;

template <typename Dir, typename Allocator>
[[nodiscard]] auto message_view(message_base<Dir, Allocator>& msg,
                                buffer_view_type_for_direction<Dir> data_buffer)
{
    msg.set_data_buffer(data_buffer);
    msg.set_control_buffer(buffer_view_type_for_direction<Dir> {});
    return message_view(msg);
}

template <typename Dir, typename Allocator>
[[nodiscard]] auto
message_view(message_base<Dir, Allocator>& msg,
             buffer_view_type_for_direction<Dir> data_buffer,
             buffer_view_type_for_direction<Dir> control_buffer)
{
    msg.set_data_buffer(data_buffer);
    msg.set_control_buffer(control_buffer);
    return message_view(msg);
}

} // namespace exios

#endif // EXIOS_MESSAGE_HPP_INCLUDED
