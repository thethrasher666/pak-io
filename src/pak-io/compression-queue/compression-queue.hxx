//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pak
{
    /// Compression job queue with worker thread pool
    /// Manages parallel compression tasks with a producer-consumer pattern
    class compression_queue
    {
    public:
        /// Job function type: takes filename and data, returns compressed data and sizes
        using job_function = std::function<void(const std::string&, const std::vector<std::uint8_t>&)>;

        /// Constructor
        /// \param worker_count Number of worker threads (0 = hardware concurrency)
        /// \param job_handler Function to execute for each job
        explicit compression_queue(std::size_t worker_count = 0, job_function job_handler = nullptr);

        /// Destructor - waits for all jobs to complete and shuts down workers
        ~compression_queue();

        /// Delete copy constructor
        compression_queue(const compression_queue&) = delete;

        /// Delete assignment operator
        auto operator=(const compression_queue&) -> compression_queue& = delete;

        /// Delete move constructor (worker threads hold this pointer)
        compression_queue(compression_queue&& other) = delete;

        /// Delete move assignment (worker threads hold this pointer)
        auto operator=(compression_queue&& other) -> compression_queue& = delete;

        /// Enqueue a compression job
        /// \param filename Name of the file being compressed
        /// \param data File data to compress
        void enqueue(const std::string& filename, std::vector<std::uint8_t> data);

        /// Wait for all pending jobs to complete
        void wait_for_completion();

        /// Shutdown the worker threads
        void shutdown();

        /// Get the number of pending jobs
        [[nodiscard]] auto pending_count() const -> std::size_t;

        /// Get the number of active jobs
        [[nodiscard]] auto active_count() const -> std::size_t;

    private:
        void worker_thread();

    private:
        struct job
        {
            std::string filename;
            std::vector<std::uint8_t> data;
        };

        job_function job_handler_;
        std::queue<job> queue_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> shutdown_requested_{ false };
        std::atomic<std::size_t> active_jobs_{ 0 };
        std::atomic<std::size_t> pending_jobs_{ 0 };
        std::vector<std::thread> workers_;
    };
} // namespace pak
