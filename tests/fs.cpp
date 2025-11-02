#include <filesystem>

#include <gtest/gtest.h>

#include "fs.hpp"

namespace fs = std::filesystem;

// Define a global test environment that sets up and tears down once.
class TestEnvironment : public testing::Environment
{
private:
    static const fs::path _TEST_DIR;

public:
    void SetUp() override
    {
        std::error_code ec;
        std::cout << "[TestEnvironment] Preparing test directory: " << _TEST_DIR << std::endl;

        // Remove old directory if it exists
        if (fs::exists(_TEST_DIR))
        {
            fs::remove_all(_TEST_DIR, ec);
            if (ec)
            {
                std::cerr << "[TestEnvironment] Warning: failed to remove old directory: " << ec.message() << std::endl;
            }
        }

        // Create a clean one
        fs::create_directories(_TEST_DIR, ec);
        if (ec)
        {
            throw std::runtime_error("Failed to create test directory: " + ec.message());
        }
    }

    void TearDown() override
    {
        // We do not really need to remove `_TEST_DIR` (or any of the test files) because (1) there will be
        // times when we need them for post-testing inspection, and (2) the `Setup()` will clean them anyway.

        // std::error_code ec;
        // std::cout << "[TestEnvironment] Cleaning up test directory: " << _TEST_DIR << std::endl;

        // fs::remove_all(_TEST_DIR, ec);
        // if (ec)
        // {
        //     std::cerr << "[TestEnvironment] Warning: failed to remove test directory: " << ec.message() << std::endl;
        // }
    }
};

const fs::path TestEnvironment::_TEST_DIR(TEST_DIR);

// Register the global environment so it runs once per test binary
testing::Environment *const test_env = testing::AddGlobalTestEnvironment(new TestEnvironment());

TEST(File, FileReadWrite)
{
    const char *data = "Hello, World!";
    std::string path(std::format("{}/FileReadWrite.txt", TEST_DIR));

    {
        auto write_file = File::create_new(path.c_str());
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
        auto read_file = File::open("/tmp/test_file.txt");
        ASSERT_TRUE(read_file.is_ok());

        std::vector<char> buffer(1 + strlen(data));
        std::span<char> read_span(buffer.data(), sizeof(buffer));

        auto read_result = read_file.unwrap().read(read_span);
        ASSERT_TRUE(read_result.is_ok());
        ASSERT_EQ(read_result.unwrap(), strlen(data));

        buffer[read_result.unwrap()] = '\0';
        ASSERT_STREQ(buffer.data(), data);
    }
}
