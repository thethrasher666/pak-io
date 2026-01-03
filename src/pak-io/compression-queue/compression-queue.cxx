//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "compression-queue.hxx"

#include <algorithm>

namespace pak
{
    compression_queue::compression_queue(std::size_t worker_count, job_function job_handler) : job_handler_(std::move(job_handler))
    {
        if (worker_count == 0)
        {
            worker_count = std::max(1u, std::thread::hardware_concurrency());
        }

        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i)
        {
            workers_.emplace_back(&compression_queue::worker_thread, this);
        }
    }

    compression_queue::~compression_queue()
    {
        shutdown();
    }

    void compression_queue::enqueue(const std::string& filename, std::vector<std::uint8_t> data)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push({ filename, std::move(data) });
        }
        ++pending_jobs_;
        cv_.notify_one();
    }

    void compression_queue::wait_for_completion()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return pending_jobs_.load() == 0 && active_jobs_.load() == 0; });
    }

    void compression_queue::shutdown()
    {
        if (shutdown_requested_.exchange(true))
        {
            return; // Already shutdown
        }

        // Wake all workers
        cv_.notify_all();

        // Wait for all workers to finish
        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        workers_.clear();
    }

    auto compression_queue::pending_count() const -> std::size_t
    {
        return pending_jobs_.load();
    }

    auto compression_queue::active_count() const -> std::size_t
    {
        return active_jobs_.load();
    }

    void compression_queue::worker_thread()
    {
        while (true)
        {
            job current_job;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return shutdown_requested_.load() || !queue_.empty(); });

                if (shutdown_requested_.load() && queue_.empty())
                {
                    break;
                }

                if (queue_.empty())
                {
                    continue;
                }

                current_job = std::move(queue_.front());
                queue_.pop();

                // Update counters while holding the lock to prevent race conditions
                --pending_jobs_;
                ++active_jobs_;
            }

            // Execute the job handler
            if (job_handler_)
            {
                job_handler_(current_job.filename, current_job.data);
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_jobs_;
            }
            cv_.notify_all(); // Notify wait_for_completion
        }
    }
} // namespace pak
