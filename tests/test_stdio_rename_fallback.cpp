#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "nmarkdown/platform/platform.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::fprintf(stderr,                                               \
                         "CHECK failed at %s:%d: %s\n",                     \
                         __FILE__, __LINE__, #condition);                      \
            ++failures;                                                        \
        }                                                                      \
    } while (false)

bool write_seed(const std::string& path, const char* value) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const std::size_t size = std::char_traits<char>::length(value);
    const bool write_ok = std::fwrite(value, 1, size, file) == size;
    const bool close_ok = std::fclose(file) == 0;
    return write_ok && close_ok;
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return bytes;
    std::uint8_t buffer[128];
    while (true) {
        const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
        bytes.insert(bytes.end(), buffer, buffer + count);
        if (count != sizeof(buffer)) break;
    }
    std::fclose(file);
    return bytes;
}

bool exists(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    std::fclose(file);
    return true;
}

void test_enosys_rename_uses_validated_rewrite() {
    const std::string path =
        std::string(NMARKDOWN_BINARY_DIR) + "/rename-enosys-state.bin";
    const std::string temporary = path + ".tmp";
    const std::string backup = path + ".bak";
    std::remove(path.c_str());
    std::remove(temporary.c_str());
    std::remove(backup.c_str());

    CHECK(write_seed(path, "previous-state"));
    const std::vector<std::uint8_t> replacement{
        0x4e, 0x4d, 0x44, 0x53, 0x01, 0x00, 0x7f, 0xa5, 0x5a};
    nmarkdown::StdioFileSystem files;
    std::string error;
    CHECK(files.write_atomic(path.c_str(), replacement.data(),
                             replacement.size(), error));
    CHECK(error.empty());
    CHECK(read_file(path) == replacement);
    CHECK(!exists(temporary));
    CHECK(!exists(backup));

    std::remove(path.c_str());
}

void test_enosys_rename_creates_missing_destination() {
    const std::string path =
        std::string(NMARKDOWN_BINARY_DIR) + "/rename-enosys-new-state.bin";
    const std::string temporary = path + ".tmp";
    const std::string backup = path + ".bak";
    std::remove(path.c_str());
    std::remove(temporary.c_str());
    std::remove(backup.c_str());

    const std::vector<std::uint8_t> replacement{
        0x4e, 0x4d, 0x44, 0x53, 0x02, 0x00, 0x00, 0xff};
    nmarkdown::StdioFileSystem files;
    std::string error;
    CHECK(files.write_atomic(path.c_str(), replacement.data(),
                             replacement.size(), error));
    CHECK(error.empty());
    CHECK(read_file(path) == replacement);
    CHECK(!exists(temporary));
    CHECK(!exists(backup));

    std::remove(path.c_str());
}

void test_mixed_rename_sequence_uses_validated_rewrite() {
    const std::string path =
        std::string(NMARKDOWN_BINARY_DIR) + "/rename-mixed-state.bin";
    const std::string temporary = path + ".tmp";
    const std::string backup = path + ".bak";
    std::remove(path.c_str());
    std::remove(temporary.c_str());
    std::remove(backup.c_str());

    CHECK(write_seed(path, "previous-state"));
    const std::vector<std::uint8_t> replacement{
        0x4e, 0x4d, 0x44, 0x53, 0x03, 0x00, 0x11, 0x22, 0x33};
    nmarkdown::StdioFileSystem files;
    std::string error;
    CHECK(files.write_atomic(path.c_str(), replacement.data(),
                             replacement.size(), error));
    CHECK(error.empty());
    CHECK(read_file(path) == replacement);
    CHECK(!exists(temporary));
    CHECK(!exists(backup));

    std::remove(path.c_str());
}

void test_mixed_rename_rewrite_failure_rolls_back() {
    const std::string path = std::string(NMARKDOWN_BINARY_DIR) +
                             "/rename-mixed-rollback-state.bin";
    const std::string temporary = path + ".tmp";
    const std::string backup = path + ".bak";
    std::remove(path.c_str());
    std::remove(temporary.c_str());
    std::remove(backup.c_str());

    CHECK(write_seed(path, "previous-state"));
    const std::vector<std::uint8_t> replacement{
        0x4e, 0x4d, 0x44, 0x53, 0x04, 0x00, 0xaa, 0x55};
    nmarkdown::StdioFileSystem files;
    std::string error;
    CHECK(!files.write_atomic(path.c_str(), replacement.data(),
                              replacement.size(), error));
    CHECK(!error.empty());
    CHECK(read_file(path) ==
          std::vector<std::uint8_t>({'p', 'r', 'e', 'v', 'i', 'o', 'u', 's',
                                     '-', 's', 't', 'a', 't', 'e'}));
    CHECK(read_file(temporary) == replacement);
    CHECK(!exists(backup));

    std::remove(path.c_str());
    std::remove(temporary.c_str());
}

}  // namespace

int main() {
    test_enosys_rename_uses_validated_rewrite();
    test_enosys_rename_creates_missing_destination();
    test_mixed_rename_sequence_uses_validated_rewrite();
    test_mixed_rename_rewrite_failure_rolls_back();
    if (failures != 0) {
        std::fprintf(stderr, "%d stdio fallback test(s) failed\n", failures);
        return 1;
    }
    std::printf("All stdio rename fallback tests passed\n");
    return 0;
}
