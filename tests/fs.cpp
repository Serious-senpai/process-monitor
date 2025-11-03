#include <filesystem>

#include <gtest/gtest.h>

#include "fs.hpp"
#include "process.hpp"

const std::string BASE_TEST_DIR = std::format("{}/{}", TEST_DIR, process::id());

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
    std::string filename;
    std::string data;

    FileReadWriteData(std::string &&filename, std::string &&data)
        : filename(filename), data(data) {}
};

class FileReadWriteTest : public testing::TestWithParam<FileReadWriteData>
{
};

TEST_P(FileReadWriteTest, ReadWrite)
{
    const auto &param = GetParam();

    std::string path(std::format("{}/{}", BASE_TEST_DIR, param.filename));
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

std::string very_long_filepath(int length)
{
    std::string result;
    result.reserve(length);

    const char *segment = "long_filename_segment/";
    auto segment_length = strlen(segment);

    const char *extension = ".txt";
    auto extension_length = strlen(extension);

    while (result.size() + segment_length + extension_length < static_cast<size_t>(length))
    {
        result += segment;
    }

    while (result.size() + extension_length < static_cast<size_t>(length))
    {
        result += 'a';
    }

    result += extension;
    return result;
}

INSTANTIATE_TEST_SUITE_P(
    FileReadWriteVariants, // test suite name
    FileReadWriteTest,     // test fixture name
    testing::Values(
        FileReadWriteData{"FileReadWrite1.txt", "Hello World!"},
        FileReadWriteData{"FileReadWrite2.txt", "Hello Sekai!"},
        FileReadWriteData{"FileReadWrite3.txt", std::string(10000000, 'A')},
        FileReadWriteData{std::format("FileReadWrite4-{}.txt", very_long_filepath(32000)), "This is a very long filename."}));
