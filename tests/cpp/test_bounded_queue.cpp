#include <gtest/gtest.h>
#include "oemaestro/BoundedQueue.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>

using OEMaestro::BoundedQueue;

TEST(BoundedQueueTest, PushPopBasic) {
    BoundedQueue<int> q(4);
    ASSERT_TRUE(q.Push(1));
    ASSERT_TRUE(q.Push(2));
    auto v1 = q.Pop();
    auto v2 = q.Pop();
    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v1, 1);
    EXPECT_EQ(*v2, 2);
}

TEST(BoundedQueueTest, PopReturnsNulloptWhenClosedAndEmpty) {
    BoundedQueue<int> q(4);
    q.Push(1);
    q.Pop();
    q.Close();
    auto v = q.Pop();
    EXPECT_FALSE(v.has_value());
}

TEST(BoundedQueueTest, PushReturnsFalseWhenClosed) {
    BoundedQueue<int> q(4);
    q.Close();
    EXPECT_FALSE(q.Push(42));
}

TEST(BoundedQueueTest, CloseUnblocksBlockedPop) {
    BoundedQueue<int> q(4);
    std::thread t([&] {
        auto v = q.Pop();
        EXPECT_FALSE(v.has_value());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.Close();
    t.join();
}

TEST(BoundedQueueTest, CloseUnblocksBlockedPush) {
    BoundedQueue<int> q(1);
    q.Push(1);
    std::thread t([&] {
        bool ok = q.Push(2);
        EXPECT_FALSE(ok);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.Close();
    t.join();
}

TEST(BoundedQueueTest, CloseDrainsRemainingItems) {
    BoundedQueue<int> q(4);
    q.Push(1);
    q.Push(2);
    q.Close();
    auto v1 = q.Pop();
    auto v2 = q.Pop();
    auto v3 = q.Pop();
    EXPECT_TRUE(v1.has_value());
    EXPECT_TRUE(v2.has_value());
    EXPECT_FALSE(v3.has_value());
}

TEST(BoundedQueueTest, MultiThreadedStress) {
    const int N = 1000;
    BoundedQueue<int> q(8);
    std::vector<int> results;
    std::mutex results_mu;

    std::thread producer([&] {
        for (int i = 0; i < N; i++) q.Push(i);
        q.Close();
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; c++) {
        consumers.emplace_back([&] {
            while (auto v = q.Pop()) {
                std::lock_guard lk(results_mu);
                results.push_back(*v);
            }
        });
    }

    producer.join();
    for (auto& t : consumers) t.join();

    std::sort(results.begin(), results.end());
    ASSERT_EQ(results.size(), static_cast<size_t>(N));
    for (int i = 0; i < N; i++) {
        EXPECT_EQ(results[i], i);
    }
}
