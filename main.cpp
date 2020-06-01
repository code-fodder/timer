#include <iostream>
#include "sync.hpp"

// Not sure what the true accuracy of the standard linux timer is. Usually it is less then 10ms
// but I have seen it up to 20-25ms and once I saw it at 40ms!
const int64_t acceptable_error_ms = 50;

class timer_test
{
public:
    void start(unsigned int time_ms, bool oneshot = true, unsigned int delay_ms = 0)
    {
        m_count = 0;
        m_delay_ms = delay_ms;
        m_tmr.start(time_ms, [this]{tick('.');}, oneshot);
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
    void tick(char chr)
    {
        std::cout << chr << std::flush;
        SLEEP_MSEC(m_delay_ms);
    }

    int m_count{0};
    thread_sync::timer m_tmr;
    thread_sync::stopwatch m_sw;
    unsigned int m_delay_ms;
};

bool test_timer()
{
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
        return false;
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
        return false;
    }
    else
    {
        std::cout << "\tPASSED: timer stopped with acceptable time margine\n";
    }

    std::cout << "\nContinuous timer loss of precision test over a longer time while work takes some time";
    time_test_value = 5000;
    test.start(100, false, 10);
    SLEEP_MSEC(time_test_value);
    test.stop();
    std::cout << "\n\tstopped at: " << test.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(test.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: timer lost too much time!\n";
        return false;
    }
    else
    {
        std::cout << "\tPASSED: timer kept time within acceptable time margine\n";
    }

    std::cout << "\nContinuous timer with work taking longer then timer interval";
    time_test_value = 4000;
    test.start(1000, false,2000);
    SLEEP_MSEC(time_test_value);
    int64_t stop_time = static_cast<int64_t>(test.get_elapsed_time());
    test.stop();
    // When we ask the timer to stop the worker will have 1000ms left of work to do
    // so we have to add that to our time deviation
    time_deviation_ms = static_cast<int64_t>(test.get_elapsed_time()) - stop_time - 1000;
    std::cout << "\n\tstoping at: " << stop_time << "ms" << std::endl;
    std::cout << "\n\tstopped at: " << test.get_elapsed_time() << "ms" << std::endl;
    // NOTE: Since worker is taking longer then the time interval the 200ms sleep errors in the worker
    // are accumulating and the timer can't do much about it. So here we just test it can stop the timer
    // in a timely manner.
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: timer lost too much time!\n";
        return false;
    }
    else
    {
        std::cout << "\tPASSED: timer kept time within acceptable time margine\n";
    }

    return true;
}

bool test_gate()
{
    thread_sync::gate test_gate;
    thread_sync::stopwatch sw;
    int64_t time_deviation_ms = 0;
    int64_t time_test_value = 0;

    std::cout << "\nTest gate opens after 100ms timeout\n";
    time_test_value = 100;
    sw.restart();
    test_gate.wait_at_gate(static_cast<unsigned int>(time_test_value));
    std::cout << "\tstopped at: " << sw.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(sw.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: gate did not open within acceptable timeout limit\n";
        return false;
    }
    else
    {
        std::cout << "\tPASSED: timer kept time within acceptable time margine\n";
    }

    std::cout << "\nTest gate waiting for a long time is opened after 200ms by open_gate() from another thread\n";
    time_test_value = 200;
    sw.restart();
    // Thread will open the gate after a delay
    std::thread th([&test_gate, &sw, time_test_value]{
        SLEEP_MSEC(time_test_value);
        test_gate.open_gate();
    });
    // for 10 times longer the thread - let the thread open the gate
    test_gate.wait_at_gate(static_cast<unsigned int>(time_test_value * 10));
    std::cout << "\tstopped at: " << sw.get_elapsed_time() << "ms" << std::endl;
    time_deviation_ms = static_cast<int64_t>(sw.get_elapsed_time()) - time_test_value;
    std::cout << "\tTime deviation: " << time_deviation_ms << "ms\n";
    if (std::abs(time_deviation_ms) > acceptable_error_ms)
    {
        std::cout << "\tFAILED: gate did not open within acceptable timeout limit\n";
        return false;
    }
    else
    {
        std::cout << "\tPASSED: timer kept time within acceptable time margine\n";
    }
    thread_sync::threading::join_thread(th);

    return true;
}

int main()
{
    // Run timer tests
    if (!test_timer())
    {
        return 1;
    }

    // Run gate test since it users timer
    if (!test_gate())
    {
        return 1;
    }

    // test lots of timers
    constexpr size_t num = 10;
    std::deque<thread_sync::timer> timers;
    for (size_t i = 0; i < num; i++)
    {
        thread_sync::timer tmr;
        timers.emplace_back(std::move(tmr));
        timers[i].start(500, [i]{ std::cout << i << std::flush; }, false);
    }
    SLEEP_MSEC(5000);

    std::cout << "\n==========================\n";
    std::cout << "All tests complete\n";
    return 0;
}