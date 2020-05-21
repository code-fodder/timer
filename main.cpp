#include <iostream>
#include "sync.hpp"

class timer_test
{
public:
    void start(unsigned int time_ms, bool oneshot = true)
    {
        m_tmr.start(time_ms, [this]{tick();}, oneshot);
        m_sw.restart();
    }

    void stop()
    {
        m_tmr.stop();
    }

    uint64_t get_elapsed_time()
    {
        return m_sw.get_elapsed_time();
    }

private:
    void tick()
    {
        std::cout << "." << std::flush;
    }

    thread_sync::timer m_tmr;
    thread_sync::stopwatch m_sw;
};

int main()
{
    const int64_t acceptable_error_ms = 30;
    int64_t time_deviation_ms = 0;
    int64_t time_test_value = 0;
    timer_test test;


    std::cout << "\nOneshot timer interrupt test";
    time_test_value = 100;
    test.start(1000);
    SLEEP_MSEC(time_test_value);
    test.stop();
    std::cout << "\n\tstopped at: " << test.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(test.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: timer did not stop in time!\n";
        return 1;
    }
    else
    {
        std::cout << "\tPASSED: timer stopped with acceptable time margine\n";
    }

    std::cout << "\nContinuous timer interrupt test";
    time_test_value = 250;
    test.start(100, false);
    SLEEP_MSEC(time_test_value);
    test.stop();
    std::cout << "\n\tstopped at: " << test.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(test.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: timer did not stop in time!\n";
        return 1;
    }
    else
    {
        std::cout << "\tPASSED: timer stopped with acceptable time margine\n";
    }

    std::cout << "\nContinuous timer loss of precision test over a longer time";
    time_test_value = 10000;
    test.start(100, false);
    SLEEP_MSEC(time_test_value);
    test.stop();
    std::cout << "\n\tstopped at: " << test.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(test.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: timer lost too much time!\n";
        return 1;
    }
    else
    {
        std::cout << "\tPASSED: timer kept time within acceptable time margine\n";
    }

    std::cout << "\n==========================\n";
    std::cout << "All tests complete\n";
    return 0;
}