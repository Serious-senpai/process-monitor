#include <config.hpp>

int main()
{
    auto cfgentry = config::load_config();
    if (cfgentry.is_ok())
    {
        std::cout << cfgentry.unwrap().size() << std::endl;
    }
    else
    {
        std::cout << cfgentry.unwrap_err().message() << std::endl;
    }
    return 0;
}
