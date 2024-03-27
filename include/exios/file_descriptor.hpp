#ifndef EXIOS_FILE_DESCRIPTOR_HPP_INCLUDED
#define EXIOS_FILE_DESCRIPTOR_HPP_INCLUDED

namespace exios
{
struct FileDescriptor
{
    static constexpr int kInvalidDescriptor = -1;

    FileDescriptor() noexcept;
    FileDescriptor(int) noexcept;
    FileDescriptor(FileDescriptor&&) noexcept;
    ~FileDescriptor();
    auto operator=(FileDescriptor&&) noexcept -> FileDescriptor&;
    auto value() const noexcept -> int const&;
    auto value() noexcept -> int&;
    friend auto swap(FileDescriptor&, FileDescriptor&) noexcept -> void;

private:
    int fd_;
};

} // namespace exios
#endif // EXIOS_FILE_DESCRIPTOR_HPP_INCLUDED
