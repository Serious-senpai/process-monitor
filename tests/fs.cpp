#include <filesystem>

#include <gtest/gtest.h>

#include "fs.hpp"

namespace fs = std::filesystem;

// Define a global test environment that sets up and tears down once.
class TestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        const fs::path dir = TEST_DIR;

        std::cout << "[TestEnvironment] Preparing test directory: " << dir << std::endl;

        // Remove old directory if it exists
        if (fs::exists(dir))
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
            if (ec)
            {
                std::cerr << "[TestEnvironment] Warning: failed to remove old directory: " << ec.message() << std::endl;
            }
        }

        // Create a clean one
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec)
        {
            throw std::runtime_error("Failed to create test directory: " + ec.message());
        }
    }

    void TearDown() override
    {
        const fs::path dir = TEST_DIR;

        std::cout << "[TestEnvironment] Cleaning up test directory: " << dir << std::endl;

        std::error_code ec;
        fs::remove_all(dir, ec);
        if (ec)
        {
            std::cerr << "[TestEnvironment] Warning: failed to remove test directory: " << ec.message() << std::endl;
        }
    }
};

// Register the global environment so it runs once per test binary
::testing::Environment *const test_env = ::testing::AddGlobalTestEnvironment(new TestEnvironment());

TEST(File, FileReadWrite)
{
    auto file = File::create_new("/tmp/test_file.txt");
    ASSERT_TRUE(file.is_ok());

    const char *data = "Hello, World!";
    std::span<const char> write_span(data, strlen(data));

    auto write_result = file.unwrap().write(write_span);
    ASSERT_TRUE(write_result.is_ok());
    ASSERT_EQ(write_result.unwrap(), strlen(data));

    auto flush_result = file.unwrap().flush();
    ASSERT_TRUE(flush_result.is_ok());

    // File goes out of scope and closes

    auto read_file = File::open("/tmp/test_file.txt");
    ASSERT_TRUE(read_file.is_ok());

    char buffer[1 + strlen(data)];
    std::span<char> read_span(buffer, sizeof(buffer));
    auto read_result = read_file.unwrap().read(read_span);
    ASSERT_TRUE(read_result.is_ok());
    ASSERT_EQ(read_result.unwrap(), strlen(data));

    buffer[read_result.unwrap()] = '\0';
    ASSERT_STREQ(buffer, data);

    // Clean up
    std::remove("/tmp/test_file.txt");
}
