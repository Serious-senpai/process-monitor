#include <iostream>

#include "listener.h"

int main(int argc, char **argv)
{
    if (initialize_logger() || set_log_level(3))
    {
        std::cerr << "Failed to initialize logger." << std::endl;
        return 1;
    }

    auto tracer = new_tracer();
    if (tracer == nullptr)
    {
        std::cerr << "Failed to create tracer. Are you root?" << std::endl;
        return 1;
    }

    Threshold threshold = {};
    threshold.threshold[Metric::DISK] = threshold.threshold[Metric::NETWORK] = 0;
    for (int i = 1; i < argc; i++)
    {
        if (set_monitor(tracer, argv[i], &threshold))
        {
            std::cerr << "Failed to set monitor for " << argv[i] << std::endl;
            free_tracer(tracer);
            return 1;
        }
    }

    while (true)
    {
        auto event = next_event(tracer, -1);
        if (event != nullptr)
        {
            std::cout << "Violation detected: PID " << event->pid
                      << ", Name: " << event->name
                      << ", Metric: " << static_cast<int>(event->metric)
                      << ", Value: " << event->value
                      << ", Threshold: " << event->threshold
                      << std::endl;
            drop_event(event);
        }
        else
        {
            std::cout << "No event received." << std::endl;
        }
    }

    free_tracer(tracer);

    return 0;
}
