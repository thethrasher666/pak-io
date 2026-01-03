//
// Copyright (c) 2026 Jamie Kenyon. All Rights Reserved.
//

#include "pak-io/memory-map/memory-map.hxx"

#include <cstring>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>

SCENARIO("memory_map handles file lifecycle", "[memory_map]")
{
    GIVEN("a temporary test file path")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_memory_map.dat";

        // Clean up any existing test file
        std::filesystem::remove(test_path);

        WHEN("creating and opening a new file for writing")
        {
            pak::memory_map mmap;
            bool result = mmap.open(test_path.string(), pak::memory_map::Mode::read_write);

            THEN("the file opens successfully")
            {
                REQUIRE(result == true);
                REQUIRE(mmap.is_open() == true);
            }

            AND_THEN("the file is created on disk")
            {
                REQUIRE(std::filesystem::exists(test_path));
            }

            AND_THEN("closing the file works")
            {
                mmap.close();
                REQUIRE(mmap.is_open() == false);
            }
        }

        // Cleanup
        std::filesystem::remove(test_path);
    }
}

SCENARIO("memory_map writes and reads data correctly", "[memory_map]")
{
    GIVEN("a memory-mapped file with test data")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_write_read.dat";
        std::filesystem::remove(test_path);

        const char* test_data = "Hello, World!";
        std::size_t test_size = std::strlen(test_data);

        WHEN("writing data sequentially")
        {
            pak::memory_map mmap;
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_write));

            std::size_t written = mmap.write(test_data, test_size);

            THEN("the correct number of bytes are written")
            {
                REQUIRE(written == test_size);
            }

            AND_THEN("the write position advances correctly")
            {
                REQUIRE(mmap.write_position() == test_size);
            }

            AND_THEN("the file size reflects the written data")
            {
                REQUIRE(mmap.size() == test_size);
            }

            // Flush and close to ensure data is persisted
            REQUIRE(mmap.flush());
            mmap.close();

            AND_THEN("reading the data back works")
            {
                pak::memory_map mmap_read;
                REQUIRE(mmap_read.open(test_path.string(), pak::memory_map::Mode::read_only));

                char buffer[256] = { 0 };
                std::size_t read = mmap_read.read(0, buffer, test_size);

                REQUIRE(read == test_size);
                REQUIRE(std::memcmp(buffer, test_data, test_size) == 0);

                mmap_read.close();
            }
        }

        std::filesystem::remove(test_path);
    }
}

SCENARIO("memory_map handles multiple sequential writes", "[memory_map]")
{
    GIVEN("a memory-mapped file")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_sequential.dat";
        std::filesystem::remove(test_path);

        WHEN("writing multiple chunks sequentially")
        {
            pak::memory_map mmap;
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_write));

            const char* chunk1 = "First";
            const char* chunk2 = "Second";
            const char* chunk3 = "Third";

            std::size_t size1 = std::strlen(chunk1);
            std::size_t size2 = std::strlen(chunk2);
            std::size_t size3 = std::strlen(chunk3);

            mmap.write(chunk1, size1);
            mmap.write(chunk2, size2);
            mmap.write(chunk3, size3);

            THEN("the write position is correct")
            {
                REQUIRE(mmap.write_position() == size1 + size2 + size3);
            }

            AND_THEN("the file size is correct")
            {
                REQUIRE(mmap.size() == size1 + size2 + size3);
            }

            // Flush and close to ensure data is persisted
            REQUIRE(mmap.flush());
            mmap.close();

            AND_THEN("reading chunks at specific offsets works")
            {
                pak::memory_map mmap_read;
                REQUIRE(mmap_read.open(test_path.string(), pak::memory_map::Mode::read_only));

                char buffer[32] = { 0 };

                // Read first chunk
                std::size_t read = mmap_read.read(0, buffer, 5);
                REQUIRE(read == 5);
                REQUIRE(std::memcmp(buffer, "First", 5) == 0);

                // Read second chunk
                std::memset(buffer, 0, sizeof(buffer));
                read = mmap_read.read(5, buffer, 6);
                REQUIRE(read == 6);
                REQUIRE(std::memcmp(buffer, "Second", 6) == 0);

                // Read third chunk
                std::memset(buffer, 0, sizeof(buffer));
                read = mmap_read.read(11, buffer, 5);
                REQUIRE(read == 5);
                REQUIRE(std::memcmp(buffer, "Third", 5) == 0);

                mmap_read.close();
            }
        }

        std::filesystem::remove(test_path);
    }
}

SCENARIO("memory_map handles flush operations", "[memory_map]")
{
    GIVEN("a memory-mapped file with data")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_flush.dat";
        std::filesystem::remove(test_path);

        pak::memory_map mmap;
        REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_write));

        const char* data = "Test data for flush";
        mmap.write(data, std::strlen(data));

        WHEN("flushing the data to disk")
        {
            bool result = mmap.flush();

            THEN("flush succeeds")
            {
                REQUIRE(result == true);
            }

            AND_THEN("the file exists and has the correct size")
            {
                REQUIRE(std::filesystem::exists(test_path));
                REQUIRE(std::filesystem::file_size(test_path) == std::strlen(data));
            }
        }

        mmap.close();
        std::filesystem::remove(test_path);
    }
}

SCENARIO("memory_map handles error conditions", "[memory_map]")
{
    GIVEN("a memory_map instance")
    {
        pak::memory_map mmap;

        WHEN("attempting to write to a closed file")
        {
            const char* data = "test";
            std::size_t written = mmap.write(data, 4);

            THEN("write fails")
            {
                REQUIRE(written == 0);
            }
        }

        WHEN("attempting to read from a closed file")
        {
            char buffer[10];
            std::size_t read = mmap.read(0, buffer, 10);

            THEN("read fails")
            {
                REQUIRE(read == 0);
            }
        }

        WHEN("attempting to flush a closed file")
        {
            bool result = mmap.flush();

            THEN("flush fails")
            {
                REQUIRE(result == false);
            }
        }

        WHEN("attempting operations with null buffer")
        {
            std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_null.dat";
            std::filesystem::remove(test_path);

            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_write));

            THEN("write with null buffer fails")
            {
                std::size_t written = mmap.write(nullptr, 10);
                REQUIRE(written == 0);
            }

            AND_THEN("read with null buffer fails")
            {
                std::size_t read = mmap.read(0, nullptr, 10);
                REQUIRE(read == 0);
            }

            mmap.close();
            std::filesystem::remove(test_path);
        }

        WHEN("attempting to write in read-only mode")
        {
            std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_readonly.dat";
            std::filesystem::remove(test_path);

            // Create a file first
            pak::memory_map mmap_write;
            REQUIRE(mmap_write.open(test_path.string(), pak::memory_map::Mode::read_write));
            mmap_write.write("test", 4);
            mmap_write.close();

            // Try to write in read-only mode
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_only));

            THEN("write fails")
            {
                const char* data = "should fail";
                std::size_t written = mmap.write(data, std::strlen(data));
                REQUIRE(written == 0);
            }

            mmap.close();
            std::filesystem::remove(test_path);
        }
    }
}

SCENARIO("memory_map handles move semantics", "[memory_map]")
{
    GIVEN("a memory-mapped file with data")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_move.dat";
        std::filesystem::remove(test_path);

        pak::memory_map mmap1;
        REQUIRE(mmap1.open(test_path.string(), pak::memory_map::Mode::read_write));

        const char* data = "Move test data";
        mmap1.write(data, std::strlen(data));

        WHEN("move constructing another instance")
        {
            pak::memory_map mmap2(std::move(mmap1));

            THEN("the new instance is open")
            {
                REQUIRE(mmap2.is_open() == true);
            }

            AND_THEN("the original instance is closed")
            {
                REQUIRE(mmap1.is_open() == false);
            }

            AND_THEN("the new instance has the correct write position")
            {
                REQUIRE(mmap2.write_position() == std::strlen(data));
            }

            mmap2.close();
        }

        WHEN("move assigning to another instance")
        {
            pak::memory_map mmap3;
            mmap3 = std::move(mmap1);

            THEN("the new instance is open")
            {
                REQUIRE(mmap3.is_open() == true);
            }

            AND_THEN("the original instance is closed")
            {
                REQUIRE(mmap1.is_open() == false);
            }

            mmap3.close();
        }

        std::filesystem::remove(test_path);
    }
}

SCENARIO("memory_map handles boundary conditions", "[memory_map]")
{
    GIVEN("a memory-mapped file with data")
    {
        std::filesystem::path test_path = std::filesystem::temp_directory_path() / "test_boundary.dat";
        std::filesystem::remove(test_path);

        pak::memory_map mmap;
        REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_write));

        const char* data = "0123456789";
        std::size_t size = std::strlen(data);
        mmap.write(data, size);
        mmap.close();

        WHEN("reading beyond file size")
        {
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_only));

            char buffer[20] = { 0 };
            std::size_t read = mmap.read(0, buffer, 100);

            THEN("only available bytes are read")
            {
                REQUIRE(read == size);
            }

            AND_THEN("the data is correct")
            {
                REQUIRE(std::memcmp(buffer, data, size) == 0);
            }

            mmap.close();
        }

        WHEN("reading from offset beyond file size")
        {
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_only));

            char buffer[10] = { 0 };
            std::size_t read = mmap.read(1000, buffer, 10);

            THEN("read fails")
            {
                REQUIRE(read == 0);
            }

            mmap.close();
        }

        WHEN("reading with zero length")
        {
            REQUIRE(mmap.open(test_path.string(), pak::memory_map::Mode::read_only));

            char buffer[10] = { 0 };
            std::size_t read = mmap.read(0, buffer, 0);

            THEN("read returns zero")
            {
                REQUIRE(read == 0);
            }

            mmap.close();
        }

        std::filesystem::remove(test_path);
    }
}
