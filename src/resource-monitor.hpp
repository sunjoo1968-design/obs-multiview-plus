#pragma once

#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>

namespace mv {
struct ResourceSnapshot {
    double cpuPercent = std::numeric_limits<double>::quiet_NaN();
    double gpuPercent = std::numeric_limits<double>::quiet_NaN();
};

// Shared only by resource tiles. No OBS or Qt objects are accessed by the worker.
class ResourceMonitor final {
public:
    static std::shared_ptr<ResourceMonitor> acquire();
    ~ResourceMonitor();
    ResourceSnapshot snapshot() const;
    ResourceMonitor(const ResourceMonitor &) = delete;
    ResourceMonitor &operator=(const ResourceMonitor &) = delete;

private:
    ResourceMonitor() = default;
    void run() noexcept;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    ResourceSnapshot snapshot_;
    bool stopping_ = false;
    std::thread worker_;
};
} // namespace mv
