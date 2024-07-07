#include "fs.hpp"

#include <physfs.h>
#include "log.hpp"

namespace rana {
namespace fs {

static const char* physfs_error()
{
    auto err = PHYSFS_getLastErrorCode();
    return (err) ? PHYSFS_getErrorByCode(err) : "unknown error";
}

void init(const char *argv0)
{
    if (!PHYSFS_init(argv0)) {
        log::err("failed to init physfs: %s", physfs_error());
        abort();
    }

    PHYSFS_mount("assets.zip", nullptr, 0);
    PHYSFS_mount("./", nullptr, 0);
}

bool readfile(std::vector<uint8_t> &buffer, const char *fn)
{
    auto f = PHYSFS_openRead(fn);
    if (!f) {
        log::err("failed to open file '%s': %s", fn, physfs_error());
        return false;
    }
    std::vector<uint8_t> buf(1024);
    size_t total_size = 0;
    while (auto size = PHYSFS_readBytes(f, buf.data()+total_size, 1024)) {
        total_size += size;
        buf.resize(buf.size() + 1024);
    }

    buf.resize(total_size);
    PHYSFS_close(f);
    buffer = buf;
    return true; 
}

size_t writefile(std::vector<uint8_t> &buffer, const char *fn)
{
    auto f = PHYSFS_openWrite(fn);
    if (!f) {
        log::err("failed to open file '%s': %s", fn, physfs_error());
        return 0;
    }
    auto bytes = PHYSFS_writeBytes(f, buffer.data(), buffer.size());
    PHYSFS_close(f);
    return bytes;
}

}
}
