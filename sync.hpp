///
/// @copyright AMD-Tech Ltd
///
/// @file sync.hpp
/// @brief threading/time synchronisation functions. This is meant to be pure c++,
/// please only include std libs.

#ifndef _SYNC_H_
#define _SYNC_H_

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <functional>

/// Sleep for seconds
#define SLEEP_SEC(secs) std::this_thread::sleep_for(std::chrono::seconds(secs))
/// Sleep for milli-seconds
#define SLEEP_MSEC(milli_secs) std::this_thread::sleep_for(std::chrono::milliseconds(milli_secs))
/// Convert seconds to milliseconds
#define SECONDS_TO_MSEC(seconds) (seconds * 1000)

/// @brief thread_sync namespace
namespace thread_sync
{
/// @brief Collection of functionality to aid the use of std::thread
class threading
{
public:
    /// @brief function that re-joins a thread if it is possible
    /// @param a_thread is a reference to the thread to be re-joined
    /// @return bool - true if re-joined, false otherwise
    static inline bool join_thread(std::thread &a_thread)
    {
        if (a_thread.joinable())
        {
            a_thread.join();
            return true;
        }
        return false;
    }
};

/// @brief Thread safe stopwatch class
class stopwatch
{
private:
    /// a mutex for accessing the time values
    mutable std::mutex m;
    /// start time value
    std::chrono::steady_clock::time_point m_start_time;
    /// end time value
    std::chrono::steady_clock::time_point m_split_time;

public:
    /// @brief Construct a new stopwatch object
    stopwatch() { restart(); }
    /// @brief Destroy the stopwatch object
    ~stopwatch() { ; }

    /// @brief Get the time stamp object
    /// @param file_path_friendly set true if you want the return string to be file-path friendly (i.e. not use chars that are not allowed in file paths)
    /// @return std::string a string representing a time in the format: "YY-MM-DD hh:mm:ss.mmm" where "mm" is minutes and "mmm" is millisecs"
    ///         if file_path_friendly is set then the formar is: "YY-MM-DD_hh_mm_ss" with no milliseconds
    static std::string get_time_stamp(bool file_path_friendly = false);

    /// @brief Resets the stopwatches time flags to "now"
    void restart()
    {
        std::lock_guard<std::mutex> lock(m);
        m_start_time = std::chrono::steady_clock::now();
        m_split_time = m_start_time;
    }

    /// @brief Stores the current time (time split)
    void take_time_split()
    {
        std::lock_guard<std::mutex> lock(m);
        m_split_time = std::chrono::steady_clock::now();
    }

    ///@brief Get the amount of time elapsed
    /// @return the total amount of time elapsed since the timer started
    uint64_t get_elapsed_time() const
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start_time).count());
    }

    /// @brief get the time split
    /// @return the time since last time split was taken
    uint64_t get_split_time() const
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_split_time).count());
    }
};


// template <typename Functor>
// class timer_queue
// {
// public:
//     timer_queue(const Functor &handler) : m_handler(handler) { }
//     // On normal destruct wait for all the queued events (default). If you want to
//     // stop asap and not wait for the queued functions, call stop(false); in your code
//     ~timer_queue() { stop(); }

//     // Add item to queue and then notify the thread
//     void add()
//     {
//         ++m_count;
//         m_cv.notify_one();
//     }

//     // Function that takes a parameter pack for any parameters that need to be passed to the
//     // timeout handler
//     template <typename ...Args>
//     void start(Args ...args)
//     {
//         m_q_thread = std::thread([this, args...]
//         {
//             m_running = true;
//             while (m_running)
//             {
//                 wait();
//                 while (m_count > 0)
//                 {
//                     --m_count;
//                     m_handler(args...);
//                 }
//             }
//         });
//     }

//     void stop(bool clear_queued_events = true)
//     {
//         m_running = false;
//         // Clear the events if we dont want them
//         if (clear_queued_events)
//         {
//             m_count = 0;
//         }
//         m_cv.notify_one();
//         threading::join_thread(m_q_thread);
//     }

// private:
//     void wait()
//     {
//         if (m_running)
//         {
//             std::mutex mtx;
//             std::unique_lock<std::mutex> lk(mtx);
//             m_cv.wait(lk);
//         }
//     }

//     std::condition_variable m_cv;
//     std::thread m_q_thread;
//     std::atomic<int> m_count{0};
//     std::atomic<bool> m_running {false};
//     Functor m_handler;
// };

/// @brief timer class to call a callback function after a specified amount of time has expired.
/// Uses std::chrono::steady_clock so that it is immune to system time changes (e.g. GPS time
/// synchronisation).
class timer
{
private:
    /// the timer thread
    std::thread timer_thread;
    /// atomic bool used to stop the timer
    std::atomic<bool> timer_running {false};
    /// condition var mutex
    std::mutex mtx;
    /// condition var used for waiting
    std::condition_variable cv;

public:
    /// @brief Construct a new timer object
    timer() = default;

    // Move constructor - this is to support moving into STLs
    // Note: the timer should not be running (or should be re-started)
    // after a move since the internal lambda will be pointing to the
    // previous thread
    timer(timer&& obj)
        : timer_thread(std::move(obj.timer_thread))
        , timer_running(obj.timer_running.load())
        , mtx()
        , cv()
    {}

    // Move assignment - this is to support moving into STLs, see note
    // for move c'tor.
    timer& operator=(timer&& obj)
    {
        threading::join_thread(timer_thread);
        timer_thread = std::move(obj.timer_thread);
        return *this;
    }

    // Non-copyable due to thread. We could make it copyable, but we don't really want that, so
    // make this move-only
    timer(timer& obj) = delete;
    timer& operator=(timer& obj) = delete;

    /// @brief Destroy the timer object - ensures the timer has stopped
    ~timer()
    {
        stop();
    }

    /// @brief Starts the timer
    /// @param interval_ms the amount of time until the timer expires in milliseconds
    /// @param timeout_handler the function callback which is called if/when the timer expires
    /// @param use_handler_thread Run timer handler function in separate thread. This can be used
    ///        if the handlers can take longer then the timer interval to stack events up. Probably
    ///        overkill for most useage since it creates another thread.
    template <typename Functor>
    void start(
        unsigned int interval_ms,
        const Functor &timeout_handler,
        bool oneshot = true)
    {
        /// Stop any timer running inc ase it was just re-started
        stop();

        /// Start the timer
        timer_running = true;
        timer_thread = std::thread([timeout_handler, interval_ms, oneshot, this]()
        {
            // Store the start time, and increment this as needed. Using a time point instead of
            // a duration makes calculating the next time trivial and there is no need to worry about
            // negative durations if the handler took longer then the timeout interval
            std::chrono::steady_clock::time_point next_time = std::chrono::steady_clock::now();
            // Keep running the timer until it is no longer running
            while (timer_running)
            {
                std::unique_lock<std::mutex> lock{mtx};
                // Calculate the time we need to wait until so that we are not losing time
                next_time += std::chrono::milliseconds(interval_ms);
                // returns true if timer was stopped, returns false if timer expired
                if (cv.wait_until(lock, next_time, [this] { return (bool)!timer_running; }))
                {
                    return;
                }

                // Timer expired - queue a call to the handler
                timeout_handler();

                // if oneshot stop the timer
                if (oneshot)
                {
                    return;
                }
            }
        });
    }

    /// @brief Stops the timer - the callback will not be called.
    void stop()
    {
        // Set the running flag to false so the timer does not continue
        timer_running = false;
        // wake the timer
        cv.notify_all();
        // Join the thread
        threading::join_thread(timer_thread);
    }
};

/// @brief This class is for use between threads, where one thread will call wait_at_gate()
/// and be blocked here until a second thread calls open_gate()
class gate
{
private:
    /// flag to store the gate state (open / closed)
    bool gate_open;
    /// flag to store if a timeout has occurred
    bool timed_out;
    /// stop watch for measuring time
    stopwatch sw;
    /// mutex to protect access accross threads
    mutable std::mutex m;
    /// condition variable to signal accross threads
    mutable std::condition_variable cv;
    /// timer for setting timeouts
    timer tmr;

public:
    /// @brief Construct a new gate object
    gate() :
        gate_open(false),
        timed_out(false),
        sw()
    { }

    /// @brief Set the gate_open flag and notifies other thread
    void open_gate()
    {
        std::lock_guard<std::mutex> lock(m);
        gate_open = true;
        sw.take_time_split();
        cv.notify_all();
    }

    /// @brief Set the gate_open flag and notifies other thread and set the timed_out flag
    void timeout_handler()
    {
        std::unique_lock<std::mutex> lock(m);
        gate_open = true;
        timed_out = true;
        sw.take_time_split();
        cv.notify_all();
    }

    /// @brief Waits until notified, or until timed out.
    /// @param timeout_ms timeout in millisecs
    /// @return false if timed out, true otherwise
    bool wait_at_gate(unsigned int timeout_ms = 0)
    {
        // Setup timeout if required
        timed_out = false;
        if (timeout_ms > 0)
        {
            tmr.start(timeout_ms, [this]() { timeout_handler(); });
        }

        std::unique_lock<std::mutex> lock(m);
        // Restart the timer
        sw.restart();
        // Wait for the gate to be opened
        cv.wait(lock, [this] { return gate_open; });
        //    --- gate is open, wait is over ---
        // Stop timer
        tmr.stop();
        // Now close the gate again for re-use
        gate_open = false;

        // Return false if timed out, true otherwise
        return !timed_out;
    }

    /// @brief gets the amount of time elapsed
    /// @return the amount of time that has elapsed
    uint64_t get_elapsed_time() const
    {
        return sw.get_split_time();
    }
};
} // namespace thread_sync

#endif // _SYNC_H_
