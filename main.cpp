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

    void stop(unsigned int time_ms)
    {
        m_tmr.start(time_ms, [this]{tick();});
        std::cout << "stopped at: " << m_sw.get_elapsed_time() << "ms" << std::endl;
    }

private:
    void tick()
    {
        std::cout << "tick at: " << m_sw.get_elapsed_time() << "ms" << std::endl;
    }

    thread_sync::timer m_tmr;
    thread_sync::stopwatch m_sw;
};

int main()
{
    timer_test test;
    std::cout << "starting timer test\n";
    test.start(100, false);
    SLEEP_SEC(5);
    std::cout << "test finished\n";
    return 0;
}