#define UNICODE
#define _UNICODE

#include <ctime>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "pdh.lib")

struct ProcessCpu {
    std::wstring name;
    double cpuPercent;
};

// ---- Battery status ----
void printBatteryStatus() {
    SYSTEM_POWER_STATUS powerStatus;

    if (GetSystemPowerStatus(&powerStatus)) {
        std::wcout << L"Battery Life Percent: " << (int)powerStatus.BatteryLifePercent << L"%\n";

        std::wcout << L"AC Power Status: ";
        if (powerStatus.ACLineStatus == 1) {
            std::wcout << L"Plugged in\n";
        } else if (powerStatus.ACLineStatus == 0) {
            std::wcout << L"On battery\n";
        } else {
            std::wcout << L"Unknown\n";
        }

        std::wcout << L"Charging: " << ((powerStatus.BatteryFlag & 8) ? L"Yes" : L"No") << L"\n";
    } else {
        std::wcout << L"Failed to get power status. Error: " << GetLastError() << L"\n";
    }
}

// ---- Top CPU-consuming processes ----
ProcessCpu getTopProcess() {
    PDH_HQUERY query;
    PdhOpenQuery(NULL, 0, &query);
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numCores = sysInfo.dwNumberOfProcessors;

    DWORD counterListSize = 0, instanceListSize = 0;
    PdhEnumObjectItems(
        NULL, NULL, L"Process",
        NULL, &counterListSize,
        NULL, &instanceListSize,
        PERF_DETAIL_WIZARD, 0
    );

    std::vector<wchar_t> counterList(counterListSize);
    std::vector<wchar_t> instanceList(instanceListSize);

    PdhEnumObjectItems(
        NULL, NULL, L"Process",
        counterList.data(), &counterListSize,
        instanceList.data(), &instanceListSize,
        PERF_DETAIL_WIZARD, 0
    );

    std::vector<std::wstring> instances;
    wchar_t* ptr = instanceList.data();
    while (*ptr) {
        instances.push_back(std::wstring(ptr));
        ptr += wcslen(ptr) + 1;
    }

    std::vector<std::pair<std::wstring, PDH_HCOUNTER>> counters;
    for (auto& instance : instances) {
        if (instance == L"_Total" || instance == L"Idle") continue;

        std::wstring path = L"\\Process(" + instance + L")\\% Processor Time";
        PDH_HCOUNTER counter;
        if (PdhAddCounter(query, path.c_str(), 0, &counter) == ERROR_SUCCESS) {
            counters.push_back({ instance, counter });
        }
    }

    PdhCollectQueryData(query);
    Sleep(1000);
    PdhCollectQueryData(query);

    std::vector<ProcessCpu> rawResults;
    for (auto& [name, counter] : counters) {
        PDH_FMT_COUNTERVALUE value;
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS) {
            double cpu = value.doubleValue / numCores;  // normalize to system-wide %
            if (cpu > 100.0) cpu = 100.0; // safety clamp
                rawResults.push_back({ name, cpu });
}
    }

    std::vector<ProcessCpu> results;
    for (auto& raw : rawResults) {
        std::wstring baseName = raw.name;
        size_t hashPos = baseName.find(L'#');
        if (hashPos != std::wstring::npos) {
            baseName = baseName.substr(0, hashPos);
        }

        bool found = false;
        for (auto& r : results) {
            if (r.name == baseName) {
                r.cpuPercent += raw.cpuPercent;
                found = true;
                break;
            }
        }
        if (!found) {
            results.push_back({ baseName, raw.cpuPercent });
        }
    }

    std::sort(results.begin(), results.end(), [](const ProcessCpu& a, const ProcessCpu& b) {
        return a.cpuPercent > b.cpuPercent;
    });

    std::wcout << L"\nTop 5 CPU-consuming processes:\n";
    int count = 0;
    for (auto& r : results) {
        if (r.cpuPercent <= 0.0) continue;
        std::wcout << r.name << L": " << r.cpuPercent << L"%\n";
        count++;
        if (count >= 5) break;
    }

    PdhCloseQuery(query);

    if (!results.empty())
        return results[0];  // the top process (already sorted descending)
    return { L"None", 0.0 };

}

// ---- Get current timestamp as a formatted string ----
std::wstring getTimestamp() {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    wchar_t buffer[64];
    wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &localTime);
    return std::wstring(buffer);
}

int main() {
    std::wcout << L"=== Battery & Power Usage Analyzer ===\n";
    std::wcout << L"Logging every 1 minute. Press Ctrl+C to stop.\n\n";

    std::wofstream csvFile("data/battery_log.csv", std::ios::app);
    // Write header only if file is new/empty
    csvFile.seekp(0, std::ios::end);
    if (csvFile.tellp() == 0) {
        csvFile << L"timestamp,battery_percent,is_charging,top_process,top_process_cpu\n";
    }

    while (true) {
        std::wstring timestamp = getTimestamp();

        SYSTEM_POWER_STATUS powerStatus;
        GetSystemPowerStatus(&powerStatus);
        int batteryPercent = (int)powerStatus.BatteryLifePercent;
        bool isCharging = (powerStatus.BatteryFlag & 8) != 0;

        std::wcout << L"[" << timestamp << L"] Battery: " << batteryPercent
                    << L"% | Charging: " << (isCharging ? L"Yes" : L"No") << L"\n";

        ProcessCpu topProcess = getTopProcess();

        // Write one row to the CSV
        csvFile << timestamp << L","
                << batteryPercent << L","
                << (isCharging ? L"Yes" : L"No") << L","
                << topProcess.name << L","
                << topProcess.cpuPercent << L"\n";
        csvFile.flush();  // make sure it's actually written to disk immediately

        std::wcout << L"----------------------------------------\n";

        Sleep(60000);  // wait 1 minute before next cycle
    }

    return 0;
}