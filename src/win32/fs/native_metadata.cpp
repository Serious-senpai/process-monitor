#include "fs.hpp"

namespace _fs_impl
{
    NativeMetadata::NativeMetadata(
        DWORD attributes,
        FILETIME creation_time,
        FILETIME last_access_time,
        FILETIME last_write_time,
        std::optional<FILETIME> change_time,
        uint64_t file_size,
        DWORD reparse_tag,
        DWORD volume_serial_number,
        DWORD number_of_links,
        uint64_t file_index)
        : NonConstructible(NonConstructibleTag::TAG),
          attributes(attributes),
          creation_time(creation_time),
          last_access_time(last_access_time),
          last_write_time(last_write_time),
          change_time(change_time),
          file_size(file_size),
          reparse_tag(reparse_tag),
          volume_serial_number(volume_serial_number),
          number_of_links(number_of_links),
          file_index(file_index) {}

    NativeFileType NativeMetadata::file_type() const
    {
        return NativeFileType(attributes, reparse_tag);
    }

    bool NativeMetadata::is_dir() const
    {
        return file_type().is_dir();
    }

    bool NativeMetadata::is_file() const
    {
        return file_type().is_file();
    }

    bool NativeMetadata::is_symlink() const
    {
        return file_type().is_symlink();
    }
}
