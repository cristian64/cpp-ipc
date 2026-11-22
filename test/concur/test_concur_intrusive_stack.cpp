#include <array>
#include <atomic>
#include <vector>

#include "../archive/test.h"

#define private public
#include "libipc/concur/intrusive_stack.h"

using namespace ipc;

TEST(intrusive_stack, construct) {
  concur::intrusive_stack<int> s;
  EXPECT_TRUE(s.empty());
}

TEST(intrusive_stack, construct_node) {
  concur::intrusive_stack<int>::node n{};
  EXPECT_TRUE(n.next.load(std::memory_order_relaxed) == nullptr);
}

TEST(intrusive_stack, copyable) {
  EXPECT_FALSE(std::is_copy_constructible<concur::intrusive_stack<int>>::value);
  EXPECT_FALSE(std::is_copy_assignable<concur::intrusive_stack<int>>::value);
}

TEST(intrusive_stack, moveable) {
  EXPECT_FALSE(std::is_move_constructible<concur::intrusive_stack<int>>::value);
  EXPECT_FALSE(std::is_move_assignable<concur::intrusive_stack<int>>::value);
}

TEST(intrusive_stack, push_one) {
  concur::intrusive_stack<int> s;
  concur::intrusive_stack<int>::node n{123};
  s.push(&n);
  EXPECT_FALSE(s.empty());
  EXPECT_TRUE(s.top_.load(std::memory_order_relaxed) == &n);
  EXPECT_TRUE(n.next.load(std::memory_order_relaxed) == nullptr);
  EXPECT_EQ(n.value, 123);
}

TEST(intrusive_stack, push_many) {
  concur::intrusive_stack<int> s;
  concur::intrusive_stack<int>::node n1{111111};
  concur::intrusive_stack<int>::node n2{222222};
  concur::intrusive_stack<int>::node n3{333333};
  s.push(&n1);
  s.push(&n2);
  s.push(&n3);
  EXPECT_FALSE(s.empty());
  EXPECT_TRUE(s.top_.load(std::memory_order_relaxed) == &n3);
  EXPECT_TRUE(n3.next.load(std::memory_order_relaxed) == &n2);
  EXPECT_TRUE(n2.next.load(std::memory_order_relaxed) == &n1);
  EXPECT_TRUE(n1.next.load(std::memory_order_relaxed) == nullptr);
  EXPECT_EQ(n1.value, 111111);
  EXPECT_EQ(n2.value, 222222);
  EXPECT_EQ(n3.value, 333333);
}

TEST(intrusive_stack, push_same) {
  concur::intrusive_stack<int> s;
  concur::intrusive_stack<int>::node n{321};
  s.push(&n);
  s.push(&n);
  EXPECT_FALSE(s.empty());
  EXPECT_TRUE(s.top_.load(std::memory_order_relaxed) == &n);
  EXPECT_TRUE(n.next.load(std::memory_order_relaxed) == &n);
  EXPECT_EQ(n.value, 321);
}

TEST(intrusive_stack, pop_empty) {
  concur::intrusive_stack<int> s;
  EXPECT_TRUE(s.pop() == nullptr);
}

TEST(intrusive_stack, pop_one) {
  concur::intrusive_stack<int> s;
  concur::intrusive_stack<int>::node n{112233};
  s.push(&n);
  EXPECT_TRUE(s.pop() == &n);
  EXPECT_TRUE(s.empty());
  EXPECT_TRUE(s.top_.load(std::memory_order_relaxed) == nullptr);
  EXPECT_TRUE(n.next.load(std::memory_order_relaxed) == nullptr);
  EXPECT_EQ(n.value, 112233);
}

TEST(intrusive_stack, pop_many) {
  concur::intrusive_stack<int> s;
  concur::intrusive_stack<int>::node n1{111111};
  concur::intrusive_stack<int>::node n2{222222};
  concur::intrusive_stack<int>::node n3{333333};
  s.push(&n1);
  s.push(&n2);
  s.push(&n3);
  EXPECT_TRUE(s.pop() == &n3);
  EXPECT_TRUE(s.pop() == &n2);
  EXPECT_TRUE(s.pop() == &n1);
  EXPECT_TRUE(s.empty());
  EXPECT_TRUE(s.top_.load(std::memory_order_relaxed) == nullptr);
  EXPECT_TRUE(n3.next.load(std::memory_order_relaxed) == &n2);
  EXPECT_TRUE(n2.next.load(std::memory_order_relaxed) == &n1);
  EXPECT_TRUE(n1.next.load(std::memory_order_relaxed) == nullptr);
  EXPECT_EQ(n1.value, 111111);
  EXPECT_EQ(n2.value, 222222);
  EXPECT_EQ(n3.value, 333333);
}

TEST(intrusive_stack, concurrent_push_pop) {
  constexpr std::size_t node_count = 64;
  constexpr unsigned thread_count = 8;
  constexpr int ops_per_thread = 200000;

  concur::intrusive_stack<int> s;
  std::array<concur::intrusive_stack<int>::node, node_count> nodes;
  std::array<std::atomic<bool>, node_count> in_use;
  for (std::size_t i = 0; i < node_count; ++i) {
    nodes[i].value = static_cast<int>(i);
    nodes[i].next.store(nullptr, std::memory_order_relaxed);
    in_use[i].store(false, std::memory_order_relaxed);
    s.push(&nodes[i]);
  }

  std::atomic<int> errors{0};
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (unsigned t = 0; t < thread_count; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i) {
        auto *n = s.pop();
        if (n == nullptr) {
          errors.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        const auto idx = static_cast<std::size_t>(n->value);
        if ((idx >= node_count) ||
            in_use[idx].exchange(true, std::memory_order_acq_rel)) {
          errors.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        std::this_thread::yield();
        in_use[idx].store(false, std::memory_order_release);
        s.push(n);
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  ASSERT_EQ(errors.load(std::memory_order_relaxed), 0);

  std::array<bool, node_count> seen{};
  for (std::size_t i = 0; i < node_count; ++i) {
    auto *n = s.pop();
    ASSERT_NE(n, nullptr);
    const auto idx = static_cast<std::size_t>(n->value);
    ASSERT_LT(idx, node_count);
    EXPECT_FALSE(seen[idx]);
    seen[idx] = true;
  }
  EXPECT_TRUE(s.empty());
}
