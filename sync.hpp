///
/// @copyright
/// Copyright in this material is vested in TRL Technology Ltd. This material is
/// issued in confidence for the purpose only for which it is supplied. It must
/// not be reproduced in whole or in part except with the consent of TRL
/// Technology Ltd and then only on the condition that this notice is included in
/// any reproduction. No information as to the contents or subject matter in
/// this material or any part thereof arising directly or indirectly therefrom
/// shall be given orally or in writing or communicated in any manner whatsoever
/// to any third party being an individual firm or company or any employee
/// thereof without the prior consent in writing of TRL Technology Ltd.
///
/// @file sync.hpp
/// @brief threading/time synchronisation functions
///

#ifndef _SYNC_H_
#define _SYNC_H_

// This is meant to be pure c++, please don't include any user-code headears
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>

/// Sleep for seconds
#define SLEEP_SEC(secs) std::this_thread::sleep_for(std::chrono::seconds(secs))
/// Sleep for milli-seconds
#define SLEEP_MSEC(milli_secs) std::this_thread::sleep_for(std::chrono::milliseconds(milli_secs))
/// Convert seconds to milliseconds
#define SECONDS_TO_MSEC(seconds) (seconds * 1000)

// Macro to safely re-join a thread with guards
//#define JOIN_THREAD(a_thread) {if (a_thread.joinable()) { a_thread.join(); }}

///
/// @brief thread_sync namespace
///
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
        ///
        /// @brief Construct a new stopwatch object
        ///
        stopwatch(){ restart(); }
        ///
        /// @brief Destroy the stopwatch object
        ///
        ~stopwatch(){;}

        ///
        /// @brief Get the time stamp object
        /// @param file_path_friendly set true if you want the return string to be file-path friendly (i.e. not use chars that are not allowed in file paths)
        /// @return std::string a string representing a time in the format: "YY-MM-DD hh:mm:ss.mmm" where "mm" is minutes and "mmm" is millisecs"
        ///         if file_path_friendly is set then the formar is: "YY-MM-DD_hh_mm_ss" with no milliseconds
        ///
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


    /// @brief timer class to call a callback function after a specified amount of time has expired
    class timer
    {
    private:
        /// the timer thread
        std::thread timer_thread;
        /// atomic bool used to stop the timer
        std::atomic<bool> timer_running;
        /// condition var mutex
        std::mutex mtx;
        /// condition var used for waiting
        std::condition_variable cv;
    public:
        /// @brief Construct a new timer object
        timer() = default;
        /// @brief Destroy the timer object - ensures the timer has stopped
        ~timer() { stop(); }

        /// @brief Starts the timer
        /// @param timeout_ms the amount of time until the timer expires in milliseconds
        /// @param timeout_handler the function callback which is called if/when the timer expires
        template <typename Functor>
        void start(unsigned int timeout_ms, const Functor &timeout_handler, bool oneshot = true)
        {
            /// Start the
            timer_running = true;
            timer_thread = std::thread([timeout_handler, timeout_ms, oneshot, this]()
            {
                thread_sync::stopwatch sw;
                uint64_t interval_ms = static_cast<uint64_t>(timeout_ms);
                // Keep a running total of the time required time to wait
                uint64_t total_time_ms = 0;
                // Keep running the timer until it is no longer running
                while (timer_running)
                {
                    // increment the total time required to wait by the interval
                    total_time_ms += interval_ms;

                    // Keep waiting until we have reached the elapsed time (in case of spurious wake)
                    // or the timer is stopped. Note the wait_for will handle timer_running = false
                    // so we don't need to check that in this loop
                    while (sw.get_elapsed_time() < total_time_ms)
                    {
                        std::unique_lock<std::mutex> lock{mtx};
                        // Re-calculate the time we need to wait for so that we are not losing time
                        // returns true if timer was stopped, returns false if timer expired
                        if (cv.wait_for(lock,
                                std::chrono::milliseconds{total_time_ms - sw.get_elapsed_time()},
                                [this]{return (bool) !timer_running;}))
                        {
                            // timer stopped
                            return;
                        }
                    }
                    // Timer expired - Call timeout handler
                    timeout_handler();

                    // if oneshot - stop the timer
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
            if (timer_thread.joinable())
            {
                timer_thread.join();
            }
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
        ///
        /// @brief Construct a new gate object
        ///
        gate() :
            gate_open(false),
            timed_out(false),
            sw()
        {;}
        ///
        /// @brief Destroy the gate object
        ///
        ~gate() { ; }

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
            cv.wait( lock, [this]{ return gate_open; });
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
}

#endif // _SYNC_H_
