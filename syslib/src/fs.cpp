#include "fs.hpp"

namespace fs
{
    OpenOptions::OpenOptions() noexcept : _inner() {}

    OpenOptions &OpenOptions::read(bool read) noexcept
    {
        _inner.read = read;
        return *this;
    }

    OpenOptions &OpenOptions::write(bool write) noexcept
    {
        _inner.write = write;
        return *this;
    }

    OpenOptions &OpenOptions::append(bool append) noexcept
    {
        _inner.append = append;
        return *this;
    }

    OpenOptions &OpenOptions::truncate(bool truncate) noexcept
    {
        _inner.truncate = truncate;
        return *this;
    }

    OpenOptions &OpenOptions::create(bool create) noexcept
    {
        _inner.create = create;
        return *this;
    }

    OpenOptions &OpenOptions::create_new(bool create_new) noexcept
    {
        _inner.create_new = create_new;
        return *this;
    }

    io::Result<File> OpenOptions::open(const path::PathBuf &path) const
    {
        auto file = SHORT_CIRCUIT(File, _fs_impl::NativeFile::open(path, _inner));
        return io::Result<File>::ok(File(std::move(file)));
    }

    File::File(_fs_impl::NativeFile &&inner) : NonConstructible(NonConstructibleTag::TAG), _inner(std::move(inner)) {}

    io::Result<File> File::open(const path::PathBuf &path)
    {
        OpenOptions options;
        return options.read(true).open(path);
    }

    io::Result<File> File::create(const path::PathBuf &path)
    {
        OpenOptions options;
        return options.write(true).create(true).truncate(true).open(path);
    }

    io::Result<File> File::create_new(const path::PathBuf &path)
    {
        OpenOptions options;
        return options.read(true).write(true).create_new(true).open(path);
    }

    io::Result<size_t> File::read(std::span<char> buffer)
    {
        return _inner.read(buffer);
    }

    io::Result<size_t> File::write(std::span<const char> buffer)
    {
        return _inner.write(buffer);
    }

    io::Result<std::monostate> File::flush()
    {
        return _inner.flush();
    }

    io::Result<uint64_t> File::seek(io::SeekFrom position)
    {
        return _inner.seek(position);
    }

    io::Result<std::monostate> DirBuilder::_create(const path::PathBuf &path) const
    {
        return _recursive ? _create_dir_all(path) : _inner.mkdir(path);
    }

    io::Result<std::monostate> DirBuilder::_create_dir_all(const path::PathBuf &path) const
    {
        if (path.empty())
        {
            return io::Result<std::monostate>::ok(std::monostate{});
        }

        auto result = _inner.mkdir(path);
        if (result.is_ok())
        {
            return result;
        }

        auto m = metadata(path);
        if (m.is_ok() && m.unwrap().is_dir())
        {
            return io::Result<std::monostate>::ok(std::monostate{});
        }

        auto parent = path.parent_path();
        if (parent.empty() || parent == path)
        {
            return io::Result<std::monostate>::err(io::Error(io::ErrorKind::Other, "Failed to create whole directory tree"));
        }

        SHORT_CIRCUIT(std::monostate, _create_dir_all(parent));
        return _inner.mkdir(path);
    }

    DirBuilder::DirBuilder() : NonConstructible(NonConstructibleTag::TAG), _inner(), _recursive(false) {}

    DirBuilder &DirBuilder::recursive(bool recursive) noexcept
    {
        _recursive = recursive;
        return *this;
    }

    io::Result<std::monostate> DirBuilder::create(const path::PathBuf &path) const
    {
        return _create(path);
    }

    Metadata::Metadata(_fs_impl::NativeMetadata &&inner)
        : NonConstructible(NonConstructibleTag::TAG), _inner(std::move(inner)) {}

    bool Metadata::is_dir() const
    {
        return _inner.is_dir();
    }

    bool Metadata::is_file() const
    {
        return _inner.is_file();
    }

    bool Metadata::is_symlink() const
    {
        return _inner.is_symlink();
    }

    io::Result<std::monostate> create_dir(const path::PathBuf &path)
    {
        return DirBuilder().create(path);
    }

    io::Result<std::monostate> create_dir_all(const path::PathBuf &path)
    {
        return DirBuilder().recursive(true).create(path);
    }

    io::Result<Metadata> metadata(const path::PathBuf &path)
    {
        auto metadata = SHORT_CIRCUIT(Metadata, _fs_impl::metadata(path));
        return io::Result<Metadata>::ok(Metadata(std::move(metadata)));
    }
}
