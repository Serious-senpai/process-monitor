#include "fs.hpp"

int main()
{
    auto file = fs::File::create("example.txt");
    return 0;
}
