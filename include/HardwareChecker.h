#pragma once
#include <QString>
#include <QThread>
#include <QSysInfo>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_LINUX)
#include <sys/sysinfo.h>
#endif

struct SystemSpecs {
    double totalRamGB = 8.0;
    double availableRamGB = 4.0;
    int cpuThreads = 4;
    QString cpuArch;
    QString osName;
};

struct CompatibilityResult {
    enum Status { Supported, TightMemory, Incompatible };
    Status status;
    QString badgeText;
    QString badgeColor;
    QString detailedReason;
    double requiredRamGB;
};

class HardwareChecker {
public:
    static SystemSpecs detectSystemSpecs() {
        SystemSpecs specs;
        specs.cpuThreads = QThread::idealThreadCount();
        specs.cpuArch = QSysInfo::currentCpuArchitecture();
        specs.osName = QSysInfo::prettyProductName();

#if defined(Q_OS_WIN)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            specs.totalRamGB = static_cast<double>(memInfo.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
            specs.availableRamGB = static_cast<double>(memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        }
#elif defined(Q_OS_MAC)
        int64_t memSize = 0;
        size_t len = sizeof(memSize);
        if (sysctlbyname("hw.memsize", &memSize, &len, nullptr, 0) == 0) {
            specs.totalRamGB = static_cast<double>(memSize) / (1024.0 * 1024.0 * 1024.0);
            specs.availableRamGB = specs.totalRamGB * 0.70;
        }
#elif defined(Q_OS_LINUX)
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            specs.totalRamGB = static_cast<double>(info.totalram * info.mem_unit) / (1024.0 * 1024.0 * 1024.0);
            specs.availableRamGB = static_cast<double>(info.freeram * info.mem_unit) / (1024.0 * 1024.0 * 1024.0);
        }
#endif
        return specs;
    }

    // Evaluates a GGUF file byte size + KV Cache Context overhead against this PC's RAM
    static CompatibilityResult checkGgufCompatibility(qint64 fileSizeBytes, int contextLength = 4096) {
        SystemSpecs specs = detectSystemSpecs();
        double modelSizeGB = static_cast<double>(fileSizeBytes) / (1024.0 * 1024.0 * 1024.0);
        double kvCacheOverheadGB = (static_cast<double>(contextLength) / 4096.0) * 0.55;
        double requiredGB = (modelSizeGB * 1.12) + kvCacheOverheadGB;

        CompatibilityResult res;
        res.requiredRamGB = requiredGB;

        if (requiredGB <= specs.totalRamGB * 0.65) {
            res.status = CompatibilityResult::Supported;
            res.badgeText = "✅ Runs Smoothly on This PC";
            res.badgeColor = "#10B981"; // Emerald green
            res.detailedReason = QString("Requires ~%1 GB RAM (Your PC has %2 GB RAM, %3 threads).")
                                     .arg(requiredGB, 0, 'f', 1)
                                     .arg(specs.totalRamGB, 0, 'f', 1)
                                     .arg(specs.cpuThreads);
        } else if (requiredGB <= specs.totalRamGB * 0.88) {
            res.status = CompatibilityResult::TightMemory;
            res.badgeText = "⚠️ Tight RAM (Close Background Apps)";
            res.badgeColor = "#F59E0B"; // Amber warning
            res.detailedReason = QString("Requires ~%1 GB RAM out of %2 GB total. May use disk swap.")
                                     .arg(requiredGB, 0, 'f', 1)
                                     .arg(specs.totalRamGB, 0, 'f', 1);
        } else {
            res.status = CompatibilityResult::Incompatible;
            res.badgeText = "❌ Too Large for This Computer";
            res.badgeColor = "#EF4444"; // Red error
            res.detailedReason = QString("Requires ~%1 GB RAM, exceeding safe limits for your %2 GB RAM.")
                                     .arg(requiredGB, 0, 'f', 1)
                                     .arg(specs.totalRamGB, 0, 'f', 1);
        }
        return res;
    }
};
