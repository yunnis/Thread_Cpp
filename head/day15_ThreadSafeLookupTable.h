#include "t_include.h"

#pragma once

// 1. 并发读不会阻塞其他线程, 并发写会阻塞
// 2. 对链表加锁不精细, 因为使用的std的list, 操作都是用同一个锁
template<typename Key, typename Value, typename Hash = std::hash<Key>>
class threadsafe_lookup_table
{
private:
    class bucket_type
    {
        // threadsafe_lookup_table 要访问 bucket_type 的私有
        friend class threadsafe_lookup_table;
        using bucket_value = std::pair<Key, Value>;
        using bucket_data = std::list<bucket_value>;
        using bucket_iterator = typename bucket_data::iterator;

        bucket_data data;
        mutable std::shared_mutex mutex;
        bucket_iterator find_entry_for(const Key& key)
        {
            return std::find_if(data.begin(), data.end(), [&](const bucket_value& item) {
                return item.first == key;
                });
        }

    public:
        Value value_for(const Key& key, Value const& default_value)
        {
            std::shared_lock<std::shared_mutex> lk(mutex);
            auto it = find_entry_for(key);
            return it == data.end() ? default_value : it->second;
        }

        void add_or_update_mapping(const Key& key, const Value& value)
        {
            std::unique_lock<std::shared_mutex> lk(mutex);
            auto it = find_entry_for(key);
            if (it == data.end())
            {
                data.push_back(bucket_value(key, value));
            }
        }

        void remove_mapping(const Key& key)
        {
            std::unique_lock<std::shared_mutex> lk(mutex);
            auto it = find_entry_for(key);
            if (it != data.end())
            {
                data.erase(it);
            }
        }
    };

private:
    std::vector<std::unique_ptr<bucket_type>> buckets;
    Hash hasher;

    bucket_type& get_bucket(const Key& key) const
    {
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }

public:
    threadsafe_lookup_table(unsigned num_buckets = 19, const Hash& hasher_ = Hash())
        : buckets(num_buckets), hasher(hasher_)
    {
        for (unsigned i = 0; i < num_buckets; ++i)
        {
            buckets[i].reset(new bucket_type);
        }
    }

    threadsafe_lookup_table(const threadsafe_lookup_table&) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table&) = delete;

    Value value_for(const Key& key, Value const& default_value = Value())
    {
        return get_bucket(key).value_for(key, default_value);
    }

    void add_or_update_mapping(const Key& key, const Value& default_value)
    {
        return get_bucket(key).add_or_update_mapping(key, default_value);
    }

    void remove_mapping(const Key& key)
    {
        return get_bucket(key).remove_mapping(key);
    }

    std::map<Key, Value> get_map()
    {
        std::vector<std::unique_lock<std::shared_mutex>> lks;
        for (unsigned i = 0; i < buckets.size(); ++i)
        {
            lks.push_back(std::unique_lock<std::shared_mutex>(buckets[i]->mutex));
        }

        std::map<Key, Value> res;
        for (unsigned i = 0; i < buckets.size(); ++i)
        {
            auto it = buckets[i]->data.begin();
            for (; it != buckets[i]->data.end(); ++it)
            {
                res.insert(*it);
            }
        }

        return res;
    }
};


void TestThreadSafeHash()
{
    std::set<int> removeSet;
    threadsafe_lookup_table<int, std::shared_ptr<MyClass>> table;
    std::thread t1([&]() {
        for (int i =0; i < 100; ++i)
        {
            auto ptr = std::make_shared<MyClass>(i);
            table.add_or_update_mapping(i, ptr);
        }
        });
    std::thread t2([&]() {
        for (int i =0; i < 100; ++i)
        {
            auto find_res = table.value_for(i, nullptr);
            if (find_res)
            {
                table.remove_mapping(i);
                removeSet.insert(i);
                ++i;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        });
    std::thread t3([&]() {
        for (int i = 100; i < 200; ++i)
        {
            auto ptr = std::make_shared<MyClass>(i);
            table.add_or_update_mapping(i, ptr);
        }
        });

    t1.join();
    t2.join();
    t3.join();

    for (auto i:removeSet)
    {
        std::cout << "remove data " << i << std::endl;
    }
    auto map = table.get_map();
    for (auto& i : map)
    {
        std::cout << "copy data " << *(i.second) << std::endl;
    }
}

void day15()
{
    TestThreadSafeHash();
}
