#include "resource-monitor.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mv {
namespace {
#ifdef _WIN32
uint64_t ticks(const FILETIME &time)
{
    return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

struct GpuQuery {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    ~GpuQuery() { if (query) PdhCloseQuery(query); }

    void initialize()
    {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) return;
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage",
                                 0, &counter) != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            counter = nullptr;
            return;
        }
        PdhCollectQueryData(query); // Rate counters need two samples.
    }

    double sample(const std::wstring &prefix)
    {
        const double unavailable = std::numeric_limits<double>::quiet_NaN();
        if (!counter || PdhCollectQueryData(query) != ERROR_SUCCESS) return unavailable;
        DWORD bytes = 0, count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr)
            != PDH_MORE_DATA || !bytes || bytes > 16 * 1024 * 1024) return unavailable;
        // Correct alignment for both structures and the appended Unicode strings.
        std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1)
                                            / sizeof(std::max_align_t));
        auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(storage.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, items)
            != ERROR_SUCCESS) return unavailable; // Instance list may change between calls.
        double busiest = unavailable;
        for (DWORD i = 0; i < count; ++i) {
            const auto &item = items[i];
            if (!item.szName || std::wstring(item.szName).compare(0, prefix.size(), prefix) != 0)
                continue;
            const auto &value = item.FmtValue;
            if ((value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA)
                || !std::isfinite(value.doubleValue)) continue;
            // GPU engines run concurrently: summing them misrepresents utilization.
            busiest = std::isnan(busiest) ? value.doubleValue : std::max(busiest, value.doubleValue);
        }
        return std::isnan(busiest) ? unavailable : std::clamp(busiest, 0.0, 100.0);
    }
};
#else
// User + system CPU time of this process in seconds, or -1 when unavailable.
double processCpuSeconds()
{
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1.0;
    return double(usage.ru_utime.tv_sec) + double(usage.ru_utime.tv_usec) / 1e6
         + double(usage.ru_stime.tv_sec) + double(usage.ru_stime.tv_usec) / 1e6;
}
#endif
} // namespace

std::shared_ptr<ResourceMonitor> ResourceMonitor::acquire()
{
    static std::mutex factoryMutex;
    static std::weak_ptr<ResourceMonitor> shared;
    std::lock_guard<std::mutex> lock(factoryMutex);
    if (auto existing = shared.lock()) return existing;
    try {
        auto monitor = std::shared_ptr<ResourceMonitor>(new ResourceMonitor);
        monitor->worker_ = std::thread([raw = monitor.get()] { raw->run(); });
        shared = monitor;
        return monitor;
    } catch (...) {
        return {}; // Monitoring must never prevent a broadcast window from opening.
    }
}

ResourceMonitor::~ResourceMonitor()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_one();
    if (worker_.joinable()) worker_.join();
}

ResourceSnapshot ResourceMonitor::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

#ifndef _WIN32
void ResourceMonitor::run() noexcept
{
    try {
        const long online = sysconf(_SC_NPROCESSORS_ONLN);
        const double processors = double(std::max<long>(1, online));
        double previousCpu = processCpuSeconds();
        auto previousTime = std::chrono::steady_clock::now();
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stopping_; })) break;
            }
            ResourceSnapshot next;
            const double cpu = processCpuSeconds();
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - previousTime).count();
            if (cpu >= 0 && previousCpu >= 0 && cpu >= previousCpu && elapsed > 0)
                next.cpuPercent = std::clamp((cpu - previousCpu) / (elapsed * processors) * 100.0, 0.0, 100.0);
            previousCpu = cpu;
            previousTime = now;
            // No per-process GPU utilization is available here; gpuPercent stays NaN,
            // which the tile shows as "unavailable" rather than as zero.
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_ = next;
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = {};
    }
}
#else
void ResourceMonitor::run() noexcept
{
    try {
        GpuQuery gpu;
        gpu.initialize();
        const std::wstring prefix = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
        const DWORD processors = std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
        FILETIME created{}, exited{}, kernel{}, user{};
        bool previousValid = GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != 0;
        uint64_t previousCpu = ticks(kernel) + ticks(user);
        auto previousTime = std::chrono::steady_clock::now();
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stopping_; })) break;
            }
            ResourceSnapshot next;
            const bool valid = GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != 0;
            const auto now = std::chrono::steady_clock::now();
            const uint64_t cpu = ticks(kernel) + ticks(user);
            const double elapsed = std::chrono::duration<double>(now - previousTime).count();
            if (valid && previousValid && cpu >= previousCpu && elapsed > 0)
                next.cpuPercent = std::clamp(static_cast<double>(cpu - previousCpu)
                                            / (elapsed * 100000.0 * processors), 0.0, 100.0);
            previousValid = valid;
            previousCpu = cpu;
            previousTime = now;
            next.gpuPercent = gpu.sample(prefix);
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_ = next;
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = {}; // Allocation/provider errors remain local to diagnostics.
    }
}
#endif
} // namespace mv
