#include "include/lru.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

std::size_t dummy_counter = 0;
const std::vector<std::string> keys = { "test1", "test2", "test3" };
const std::vector<std::string> values = { "val1", "val2", "val3" };

struct TestingObject
{
    TestingObject(const std::string& name,
                  std::size_t& c_counter,
                  std::size_t& m_counter) :
        name(name),
        copy_counter(c_counter),
        move_counter(m_counter)
    {}

    ~TestingObject() = default;

    TestingObject(const TestingObject& other) :
        copy_counter(other.copy_counter),
        move_counter(other.move_counter)
    {
        if (this != &other)
        {
            name = other.name;
            copy_counter++;
        }
    }

    TestingObject(TestingObject&& other) noexcept :
        copy_counter(other.copy_counter),
        move_counter(other.move_counter)
    {
        name = std::move(other.name);
        move_counter++;
    }

    std::string name;
    std::size_t& copy_counter;
    std::size_t& move_counter;
};

struct TestingObjectHasher
{
    std::size_t operator() (const TestingObject& obj) const
    {
        return std::hash<std::string>()(obj.name);
    }
};

struct TestingObjectEqual
{
    bool operator() (const TestingObject& lhs, const TestingObject& rhs) const
    {
        return lhs.name == rhs.name;
    }
};

TEST_CASE("Creation", "[creation]")
{
    SECTION("Happy Flow")
    {
        std::size_t capacity = 10;
        lru<std::string, int> lru_cache(capacity);
        REQUIRE(lru_cache.max_size() == capacity);
    }

    SECTION("Custom Key")
    {
        lru<TestingObject, int, TestingObjectHasher, TestingObjectEqual> custom_lru(10);
    }
}

TEST_CASE("Insertion", "[insertion]")
{
    lru<std::string, std::string> cache(2);

    SECTION("Happy Flow")
    {
        cache.insert(keys[0], values[0]);
        REQUIRE(cache.size() == 1);

        auto iter = cache.find(keys[0]);
        REQUIRE(iter != cache.end());
        REQUIRE(iter->second == values[0]);
    }

    SECTION("Moves")
    {
        SECTION("Key Move")
        {
            auto key = keys[0];
            REQUIRE(cache.insert(std::move(key), values[0]));
        }

        SECTION("Value Move")
        {
            auto value = values[0];
            REQUIRE(cache.insert(keys[0], std::move(value)));
        }

        SECTION("Key and Value Move")
        {
            auto key = keys[0];
            auto value = values[0];
            REQUIRE(cache.insert(std::move(key), std::move(value)));
        }

        REQUIRE(cache.size() == 1);
        auto iter = cache.find(keys[0]);
        REQUIRE(iter != cache.end());
        REQUIRE(iter->second == values[0]);
    }

    SECTION("Pruning")
    {
        for (int i = 0; i < values.size(); i++)
        {
            cache.insert(keys[i], values[i]);
        }

        REQUIRE(cache.find(keys[0]) == cache.end());
    }

    SECTION("Double Insertion")
    {
        cache.insert(keys[0], values[0]);
        REQUIRE_FALSE(cache.insert(keys[0], values[1]));

        REQUIRE(cache.size() == 1);
        auto iter = cache.find(keys[0]);
        REQUIRE(iter != cache.end());
        REQUIRE(iter->second == values[0]);
    }

    SECTION("insert_or_assign - Insertion")
    {
        REQUIRE(cache.insert_or_assign(keys[0], values[0]));
        REQUIRE(cache.size() == 1);

        auto iter = cache.find(keys[0]);
        REQUIRE(iter != cache.end());
        REQUIRE(iter->second == values[0]);
    }

    SECTION("insert_or_assign - Assignment")
    {
        REQUIRE(cache.insert_or_assign(keys[0], values[0]));
        REQUIRE_FALSE(cache.insert_or_assign(keys[0], values[1]));
        
        REQUIRE(cache.size() == 1);

        auto iter = cache.find(keys[0]);
        REQUIRE(iter != cache.end());
        REQUIRE(iter->second == values[1]);
    }

    SECTION("Custom LRU insertion")
    {
        TestingObject test("test", dummy_counter, dummy_counter);
        TestingObject test2("test2", dummy_counter, dummy_counter);

        lru<TestingObject, int, TestingObjectHasher, TestingObjectEqual> custom_lru(1);

        REQUIRE(custom_lru.insert(test, 10));
        REQUIRE(custom_lru.insert(test2, 20));

        REQUIRE_FALSE(custom_lru.contains(test));
        
        auto iter = custom_lru.find(test2);
        REQUIRE(iter != custom_lru.end());
        REQUIRE(iter->second == 20);
    }
}

TEST_CASE("Get", "[Get]")
{
    lru<std::string, std::string> cache(2);
    cache.insert(keys[0], values[0]);

    SECTION("Happy Flow")
    {
        SECTION("get")
        {
            auto val = cache.get(keys[0]);
            REQUIRE(val.has_value());
        }

        SECTION("get_copy")
        {
            auto val = cache.get_copy(keys[0]);
            REQUIRE(val.has_value());
        }
    }

    SECTION("Invalid Value")
    {
        SECTION("get")
        {
            auto val = cache.get("non-existent");
            REQUIRE(!val.has_value());
        }

        SECTION("get_copy")
        {
            auto val = cache.get_copy("non-existent");
            REQUIRE(!val.has_value());
        }
    }
}

TEST_CASE("Update", "[update]")
{
    lru<std::string, std::string> cache(2);
    cache.insert(keys[0], values[0]);
    std::string new_value = "new";

    SECTION("try_update")
    {
        cache.try_update(keys[0],
            [&](std::string& value)
            {
                value = new_value;
            }
        );
    }

    SECTION("for_each")
    {
        CHECK(cache.insert(keys[1], values[1]));

        cache.for_each(
            [&](auto& pair)
            {
                pair.second = new_value;
            }
        );
    }

    for (auto& [__, value] : cache)
    {
        REQUIRE(value == new_value);
    }
}

TEST_CASE("Performance")
{
    std::size_t copy_counter = 0;
    std::size_t move_counter = 0;
    TestingObject test("test", copy_counter, move_counter);
    TestingObject dummy("dummy", dummy_counter, dummy_counter);

    lru<TestingObject, TestingObject, TestingObjectHasher, TestingObjectEqual> cache(2);

    SECTION("Copy")
    {
        SECTION("Key Copy")
        {
            REQUIRE(cache.insert(test, dummy));
        }

        SECTION("Value Copy")
        {
            REQUIRE(cache.insert(dummy, test));
        }

        REQUIRE(copy_counter == 1);
        REQUIRE(move_counter == 0);
    }

    SECTION("Move")
    {
        SECTION("Key Move")
        {
            REQUIRE(cache.insert(std::move(test), dummy));
        }

        SECTION("Value Move")
        {
            REQUIRE(cache.insert(dummy, std::move(test)));
        }

        REQUIRE(copy_counter == 0);
        REQUIRE(move_counter == 1);
    }
}

TEST_CASE("Edge Cases", "[edge]")
{
    SECTION("Fundamental Type Key")
    {
        lru<int, int> int_lru(2);
        REQUIRE(int_lru.insert(1, 2));
        REQUIRE(int_lru.size() == 1);

        auto iter = int_lru.find(1);
        REQUIRE(iter != int_lru.end());
        REQUIRE(iter->second == 2);
    }
}
