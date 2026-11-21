#pragma once

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "libipc/imp/log.h"
#include "libipc/platform/posix/shm_name.h"
#include "libipc/shm.h"

namespace ipc {
namespace detail {
namespace sync {

class semaphore {
    ipc::shm::handle shm_;
    sem_t *h_ = SEM_FAILED;
    std::string sem_name_;  // Store the actual semaphore name used

public:
    semaphore() = default;
    ~semaphore() noexcept = default;

    sem_t *native() const noexcept {
        return h_;
    }

    bool valid() const noexcept {
        return h_ != SEM_FAILED;
    }

    bool open(const char *name, std::uint32_t count) noexcept {
        LIBIPC_LOG();
        close();
        if (!name) return false;
        if (!shm_.acquire(name, 1)) {
            log.error("[open_semaphore] fail shm.acquire: ", name);
            return false;
        }
        // Use a separate namespace for semaphores to avoid conflicts with shm.
        const std::string name_with_suffix = std::string{name} + "_sem";
        sem_name_ = ipc::posix_::detail::make_shm_name(name_with_suffix.c_str());
        h_ = ::sem_open(sem_name_.c_str(), O_CREAT, 0666, static_cast<unsigned>(count));
        if (h_ == SEM_FAILED) {
            log.error("fail sem_open[", errno, "]: ", sem_name_);
            sem_name_.clear();
            shm_.release();
            return false;
        }
        return true;
    }

    void close() noexcept {
        LIBIPC_LOG();
        const bool was_valid = valid();
        if (was_valid) {
            if (::sem_close(h_) != 0)
                log.error("fail sem_close[", errno, "]");
            h_ = SEM_FAILED;
        }
        if (shm_.valid()) {
            const auto ref = shm_.release();
            if (was_valid && ref == 1 && !sem_name_.empty()) {
                if (::sem_unlink(sem_name_.c_str()) != 0 && errno != ENOENT) {
                    log.error("fail sem_unlink[", errno, "]: ", sem_name_);
                }
            }
        }
        sem_name_.clear();
    }

    void clear() noexcept {
        LIBIPC_LOG();
        if (valid()) {
            if (::sem_close(h_) != 0) {
                log.error("fail sem_close[", errno, "]");
            }
            h_ = SEM_FAILED;
        }
        if (!sem_name_.empty()) {
            if (::sem_unlink(sem_name_.c_str()) != 0 && errno != ENOENT) {
                log.error("fail sem_unlink[", errno, "]: ", sem_name_);
            }
            sem_name_.clear();
        }
        shm_.clear();
    }

    static void clear_storage(const char *name) noexcept {
        LIBIPC_LOG();
        if (!name) return;

        // Construct the semaphore name same way as open() does
        const std::string name_with_suffix = std::string{name} + "_sem";
        const std::string sem_name = ipc::posix_::detail::make_shm_name(name_with_suffix.c_str());
        if (::sem_unlink(sem_name.c_str()) != 0 && errno != ENOENT) {
            log.error("fail sem_unlink[", errno, "]: ", sem_name);
        }
        ipc::shm::handle::clear_storage(name);
    }

    bool wait(std::uint64_t tm) noexcept {
        LIBIPC_LOG();
        if (!valid()) return false;
        if (tm == invalid_value) {
            if (::sem_wait(h_) != 0) {
                log.error("fail sem_wait[", errno, "]");
                return false;
            }
            return true;
        }

        // macOS has no sem_timedwait; emulate it with a bounded try-wait loop.
        auto const deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(tm);
        for (;;) {
            if (::sem_trywait(h_) == 0) return true;
            if (errno != EAGAIN) {
                log.error("fail sem_trywait[", errno, "]");
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    bool post(std::uint32_t count) noexcept {
        LIBIPC_LOG();
        if (!valid()) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (::sem_post(h_) != 0) {
                log.error("fail sem_post[", errno, "]");
                return false;
            }
        }
        return true;
    }
};

} // namespace sync
} // namespace detail
} // namespace ipc
