/**
 * \file libconcur/intrusive_stack.h
 * \author mutouyun (orz@orzz.org)
 * \brief Define concurrent intrusive stack.
 */
#pragma once

#include <atomic>
#include <mutex>

#include "libipc/rw_lock.h"

namespace ipc {
namespace concur {

/// \brief Intrusive stack node.
/// \tparam T The type of the value.
template <typename T>
struct intrusive_node {
  T value;
  std::atomic<intrusive_node *> next;
};

/// \brief Intrusive stack.
/// \tparam T The type of the value.
/// \tparam Node The type of the node.
template <typename T, typename Node = intrusive_node<T>>
class intrusive_stack {
public:
  using node = Node;

private:
  std::atomic<node *> top_{nullptr};
  ipc::spin_lock lock_;

public:
  intrusive_stack(intrusive_stack const &) = delete;
  intrusive_stack(intrusive_stack &&) = delete;
  intrusive_stack &operator=(intrusive_stack const &) = delete;
  intrusive_stack &operator=(intrusive_stack &&) = delete;

  constexpr intrusive_stack() noexcept = default;

  bool empty() const noexcept {
    return top_.load(std::memory_order_acquire) == nullptr;
  }

  void push(node *n) noexcept {
    std::lock_guard guard{lock_};
    node *const old_top = top_.load(std::memory_order_relaxed);
    n->next.store(old_top, std::memory_order_relaxed);
    top_.store(n, std::memory_order_release);
  }

  node *pop() noexcept {
    std::lock_guard guard{lock_};
    node *const old_top = top_.load(std::memory_order_relaxed);
    if (old_top == nullptr) {
      return nullptr;
    }
    top_.store(old_top->next.load(std::memory_order_relaxed), std::memory_order_release);
    return old_top;
  }
};

} // namespace concur
} // namespace ipc
