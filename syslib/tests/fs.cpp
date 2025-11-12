#include <filesystem>

#include <gtest/gtest.h>

#include "fs.hpp"
#include "path.hpp"
#include "process.hpp"

const path::PathBuf BASE_TEST_DIR = std::format("{}/{}", TEST_DIR, process::id());

// Define a global test environment that sets up and tears down once.
class TestEnvironment : public testing::Environment
{
public:
    void SetUp() override
    {
        std::error_code ec;
        std::cout << "[TestEnvironment] Preparing test directory: " << BASE_TEST_DIR << std::endl;

        // Remove old directory if it exists
        if (std::filesystem::exists(BASE_TEST_DIR))
        {
            std::filesystem::remove_all(BASE_TEST_DIR, ec);
            if (ec)
            {
                std::cerr << "[TestEnvironment] Warning: failed to remove old directory: " << ec.message() << std::endl;
            }
        }

        // Create a clean one
        std::filesystem::create_directories(BASE_TEST_DIR, ec);
        if (ec)
        {
            throw std::runtime_error("Failed to create test directory: " + ec.message());
        }
    }

    void TearDown() override
    {
        // We do not really need to remove `BASE_TEST_DIR` (or any of the test files) because (1) there will be
        // times when we need them for post-testing inspection, and (2) the `Setup()` will clean them anyway.

        // std::error_code ec;
        // std::cout << "[TestEnvironment] Cleaning up test directory: " << BASE_TEST_DIR << std::endl;

        // std::filesystem::remove_all(BASE_TEST_DIR, ec);
        // if (ec)
        // {
        //     std::cerr << "[TestEnvironment] Warning: failed to remove test directory: " << ec.message() << std::endl;
        // }
    }
};

// Register the global environment so it runs once per test binary
testing::Environment *const test_env = testing::AddGlobalTestEnvironment(new TestEnvironment());

class FileReadWriteData
{
public:
    path::PathBuf filename;
    std::string data;

    FileReadWriteData(path::PathBuf &&filename, std::string &&data)
        : filename(filename), data(data) {}
};

class FileReadWriteTest : public testing::TestWithParam<FileReadWriteData>
{
};

TEST_P(FileReadWriteTest, ReadWrite)
{
    const auto &param = GetParam();

    auto path = BASE_TEST_DIR / param.filename;
    std::cout << "Testing file at " << path << std::endl;

    auto mkdir = fs::create_dir_all(path.parent_path());
    if (!mkdir.is_ok())
    {
        std::cout << "Warning: Failed to create parent directories: " << std::move(mkdir).into_err().message() << std::endl;
    }

    const char *data = param.data.c_str();

    {
        auto write_file = fs::File::create_new(path.c_str());
        ASSERT_TRUE(write_file.is_ok());

        std::span<const char> write_span(data, strlen(data));

        auto write_result = write_file.unwrap().write(write_span);
        ASSERT_TRUE(write_result.is_ok());
        ASSERT_EQ(write_result.unwrap(), strlen(data));

        auto flush_result = write_file.unwrap().flush();
        ASSERT_TRUE(flush_result.is_ok());
    }
    // File goes out of scope and closes

    {
        auto read_file = fs::File::open(path.c_str());
        ASSERT_TRUE(read_file.is_ok());

        std::vector<char> buffer(1 + strlen(data));
        std::span<char> read_span(buffer.data(), buffer.size());

        auto read_result = read_file.unwrap().read(read_span);
        ASSERT_TRUE(read_result.is_ok());
        ASSERT_EQ(read_result.unwrap(), strlen(data));

        buffer[read_result.unwrap()] = '\0';
        ASSERT_STREQ(buffer.data(), data);
    }
}

path::PathBuf very_long_filepath(int subdir_length, int levels)
{
    path::PathBuf result = "FileReadWrite45";
    for (int i = 0; i < levels; ++i)
    {
        result /= std::string(subdir_length, 'a' + (i % 26));
    }

    result += ".txt";
    return result;
}

INSTANTIATE_TEST_SUITE_P(
    FileReadWriteVariants, // test suite name
    FileReadWriteTest,     // test fixture name
    testing::Values(
        FileReadWriteData{"FileReadWrite1.txt", "Hello World!"},
        FileReadWriteData{"FileReadWrite2.txt", "Hello Sekai!"},
        FileReadWriteData{"FileReadWrite3.txt", std::string(10000000, 'A')},
#ifdef WIN32
        // Linux has a smaller MAX_PATH limit, so we skip these tests there.
        FileReadWriteData{very_long_filepath(150, 200), "This is a very long filename."},
#endif
        FileReadWriteData{very_long_filepath(15, 200), "This is a very long filename."}));

TEST(ListDirectory, CreateList)
{
    auto dir_path = BASE_TEST_DIR / "ls" / "layer1" / "layer2";
    fs::create_dir_all(dir_path);

    auto metadata = fs::metadata(dir_path);
    ASSERT_TRUE(metadata.is_ok());
    ASSERT_TRUE(metadata.unwrap().is_dir());

    for (int i = 0; i < 10; i++)
    {
        auto file_path = dir_path / std::format("file-{}.txt", i);
        auto create_result = fs::File::create_new(file_path);
        ASSERT_TRUE(create_result.is_ok());
    }

    auto read_dir = fs::read_dir(std::move(dir_path));
    auto entry = read_dir.begin();
    ASSERT_TRUE(entry.is_ok());

    auto dir_entry = std::move(entry).into_ok();

    int file_count = 0;
    while (true)
    {
        auto next = dir_entry.next();
        ASSERT_TRUE(next.is_ok());
        file_count++;
        if (!next.unwrap())
        {
            break;
        }
    }

    ASSERT_EQ(file_count, 12);
}
