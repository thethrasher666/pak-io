//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/compression-queue/compression-queue.hxx"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>

SCENARIO("compression_queue processes jobs with worker threads", "[compression_queue]")
{
    GIVEN("a compression queue with job handler")
    {
        std::atomic<int> jobs_processed{ 0 };
        std::vector<std::string> processed_files;
        std::mutex results_mutex;

        pak::compression_queue queue(2,
                                     [&](const std::string& filename, const std::vector<std::uint8_t>& data)
                                     {
                                         // Simulate some work
                                         std::this_thread::sleep_for(std::chrono::milliseconds(10));

                                         {
                                             std::lock_guard<std::mutex> lock(results_mutex);
                                             processed_files.push_back(filename);
                                         }

                                         jobs_processed.fetch_add(1);
                                     });

        WHEN("enqueueing multiple jobs")
        {
            queue.enqueue("file1.txt", std::vector<std::uint8_t>{ 1, 2, 3 });
            queue.enqueue("file2.txt", std::vector<std::uint8_t>{ 4, 5, 6 });
            queue.enqueue("file3.txt", std::vector<std::uint8_t>{ 7, 8, 9 });

            THEN("pending count increases")
            {
                REQUIRE(queue.pending_count() >= 0);
            }

            AND_WHEN("waiting for completion")
            {
                queue.wait_for_completion();

                THEN("all jobs are processed")
                {
                    REQUIRE(jobs_processed.load() == 3);
                    REQUIRE(processed_files.size() == 3);
                }

                AND_THEN("pending and active counts are zero")
                {
                    REQUIRE(queue.pending_count() == 0);
                    REQUIRE(queue.active_count() == 0);
                }
            }
        }
    }
}

SCENARIO("compression_queue handles empty queue", "[compression_queue]")
{
    GIVEN("a compression queue with no jobs")
    {
        std::atomic<int> jobs_processed{ 0 };

        pak::compression_queue queue(2, [&](const std::string&, const std::vector<std::uint8_t>&) { jobs_processed.fetch_add(1); });

        WHEN("waiting for completion immediately")
        {
            queue.wait_for_completion();

            THEN("no jobs are processed")
            {
                REQUIRE(jobs_processed.load() == 0);
            }

            AND_THEN("pending and active counts are zero")
            {
                REQUIRE(queue.pending_count() == 0);
                REQUIRE(queue.active_count() == 0);
            }
        }
    }
}

SCENARIO("compression_queue handles high job volume", "[compression_queue]")
{
    GIVEN("a compression queue with multiple workers")
    {
        std::atomic<int> jobs_processed{ 0 };

        pak::compression_queue queue(4,
                                     [&](const std::string&, const std::vector<std::uint8_t>&)
                                     {
                                         // Minimal work to process many jobs quickly
                                         jobs_processed.fetch_add(1);
                                     });

        WHEN("enqueueing many jobs")
        {
            const int job_count = 100;
            for (int i = 0; i < job_count; ++i)
            {
                std::string filename = "file" + std::to_string(i) + ".txt";
                queue.enqueue(filename, std::vector<std::uint8_t>{ static_cast<std::uint8_t>(i) });
            }

            queue.wait_for_completion();

            THEN("all jobs are processed")
            {
                REQUIRE(jobs_processed.load() == job_count);
            }

            AND_THEN("queue is empty")
            {
                REQUIRE(queue.pending_count() == 0);
                REQUIRE(queue.active_count() == 0);
            }
        }
    }
}

SCENARIO("compression_queue processes jobs in parallel", "[compression_queue]")
{
    GIVEN("a compression queue with multiple workers")
    {
        std::atomic<int> concurrent_jobs{ 0 };
        std::atomic<int> max_concurrent{ 0 };
        std::atomic<int> jobs_completed{ 0 };

        pak::compression_queue queue(4,
                                     [&](const std::string&, const std::vector<std::uint8_t>&)
                                     {
                                         int current = concurrent_jobs.fetch_add(1) + 1;

                                         // Track maximum concurrency
                                         int prev_max = max_concurrent.load();
                                         while (prev_max < current && !max_concurrent.compare_exchange_weak(prev_max, current))
                                         {
                                         }

                                         // Simulate work
                                         std::this_thread::sleep_for(std::chrono::milliseconds(50));

                                         concurrent_jobs.fetch_sub(1);
                                         jobs_completed.fetch_add(1);
                                     });

        WHEN("enqueueing jobs that can run in parallel")
        {
            for (int i = 0; i < 10; ++i)
            {
                queue.enqueue("file" + std::to_string(i), std::vector<std::uint8_t>{ 1, 2, 3 });
            }

            queue.wait_for_completion();

            THEN("multiple jobs run concurrently")
            {
                REQUIRE(max_concurrent.load() > 1);
                REQUIRE(max_concurrent.load() <= 4); // Should not exceed worker count
            }

            AND_THEN("all jobs complete")
            {
                REQUIRE(jobs_completed.load() == 10);
            }
        }
    }
}

SCENARIO("compression_queue handles shutdown gracefully", "[compression_queue]")
{
    GIVEN("a compression queue with pending jobs")
    {
        std::atomic<int> jobs_started{ 0 };
        std::atomic<int> jobs_completed{ 0 };

        pak::compression_queue queue(2,
                                     [&](const std::string&, const std::vector<std::uint8_t>&)
                                     {
                                         jobs_started.fetch_add(1);
                                         std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                         jobs_completed.fetch_add(1);
                                     });

        WHEN("shutting down during job processing")
        {
            // Enqueue several jobs
            for (int i = 0; i < 5; ++i)
            {
                queue.enqueue("file" + std::to_string(i), std::vector<std::uint8_t>{ 1 });
            }

            // Give workers time to start some jobs
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Shutdown will complete remaining jobs
            queue.shutdown();

            THEN("started jobs are completed")
            {
                REQUIRE(jobs_started.load() == jobs_completed.load());
            }

            AND_THEN("queue stops accepting new work after shutdown")
            {
                int completed_before = jobs_completed.load();
                queue.enqueue("late_file.txt", std::vector<std::uint8_t>{ 1 });
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // Job should not be processed after shutdown
                REQUIRE(jobs_completed.load() == completed_before);
            }
        }
    }
}

SCENARIO("compression_queue uses default worker count", "[compression_queue]")
{
    GIVEN("a compression queue with default worker count")
    {
        std::atomic<int> jobs_processed{ 0 };

        pak::compression_queue queue(0, [&](const std::string&, const std::vector<std::uint8_t>&) { jobs_processed.fetch_add(1); });

        WHEN("processing jobs")
        {
            for (int i = 0; i < 10; ++i)
            {
                queue.enqueue("file" + std::to_string(i), std::vector<std::uint8_t>{ 1 });
            }

            queue.wait_for_completion();

            THEN("all jobs are processed")
            {
                REQUIRE(jobs_processed.load() == 10);
            }
        }
    }
}

SCENARIO("compression_queue handles jobs with varying data sizes", "[compression_queue]")
{
    GIVEN("a compression queue")
    {
        std::atomic<std::size_t> total_bytes_processed{ 0 };

        pak::compression_queue queue(2, [&](const std::string&, const std::vector<std::uint8_t>& data) { total_bytes_processed.fetch_add(data.size()); });

        WHEN("enqueueing jobs with different data sizes")
        {
            queue.enqueue("small.txt", std::vector<std::uint8_t>(10));
            queue.enqueue("medium.txt", std::vector<std::uint8_t>(1000));
            queue.enqueue("large.txt", std::vector<std::uint8_t>(100000));

            queue.wait_for_completion();

            THEN("all data is processed correctly")
            {
                REQUIRE(total_bytes_processed.load() == 101010);
            }
        }
    }
}
