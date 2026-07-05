#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cmath>
#include <iomanip>

struct FileState {
    LARGE_INTEGER createTime;
    std::wstring fileName;
};

void PrintTime(LARGE_INTEGER fileTime) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime((FILETIME*)&fileTime, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    std::wcout << std::setfill(L'0') << std::setw(2) << stLocal.wMonth << L"/" << std::setw(2) << stLocal.wDay << L"/" << stLocal.wYear << L" "
    << std::setw(2) << stLocal.wHour << L":" << std::setw(2) << stLocal.wMinute << L":" << std::setw(2) << stLocal.wSecond;
}

LARGE_INTEGER GetSystemBootTime() {
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);

    LARGE_INTEGER liNow;
    liNow.LowPart = ftNow.dwLowDateTime;
    liNow.HighPart = ftNow.dwHighDateTime;

    ULONGLONG uptime100ns = GetTickCount64() * 10000ULL;

    LARGE_INTEGER bootTime;
    bootTime.QuadPart = liNow.QuadPart - uptime100ns;

    return bootTime;
}

double CalculateEntropy(const std::wstring& str) {
    std::unordered_map<wchar_t, int> frequencies;
    for (wchar_t c : str) frequencies[c]++;
    double entropy = 0.0;
    double length = static_cast<double>(str.length());
    for (const auto& pair : frequencies) {
        double p = pair.second / length;
        entropy -= p * log2(p);
    }
    return entropy;
}

bool IsStrictlyAlphabetic(const std::wstring& str) {
    for (wchar_t c : str) {
        if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z'))) return false;
    }
    return true;
}

bool HasRandomCaseRatio(const std::wstring& str) {
    if (str.empty()) return false;
    int upperCount = 0;
    for (wchar_t c : str) {
        if (c >= L'A' && c <= L'Z') upperCount++;
    }
    double upperRatio = static_cast<double>(upperCount) / str.length();
    return (upperRatio >= 0.35 && upperRatio <= 0.65);
}

void ScanUSNJournal(char driveLetter) {
    std::string volPath = "\\\\.\\";
    volPath += driveLetter;
    volPath += ":";

    HANDLE hVol = CreateFileA(volPath.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hVol == INVALID_HANDLE_VALUE) {
        return;
    }

    USN_JOURNAL_DATA_V0 usnJournalData;
    DWORD bytesReturned;

    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
        &usnJournalData, sizeof(usnJournalData), &bytesReturned, NULL)) {
        CloseHandle(hVol);
        return;
    }

    LARGE_INTEGER bootTime = GetSystemBootTime();

    READ_USN_JOURNAL_DATA_V0 readData = { 0 };
    readData.StartUsn = 0; // we start from 0 but ignore anything older than boot time
    readData.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE;
    readData.ReturnOnlyOnClose = FALSE;
    readData.Timeout = 0;
    readData.BytesToWaitFor = 0;
    readData.UsnJournalID = usnJournalData.UsnJournalID;

    BYTE buffer[65536];
    std::unordered_map<DWORDLONG, FileState> fileTracker;

    while (true) {
        if (!DeviceIoControl(hVol, FSCTL_READ_USN_JOURNAL, &readData, sizeof(readData),
            buffer, 65536, &bytesReturned, NULL)) {
            break;
        }

        if (bytesReturned < sizeof(USN)) break;

        DWORD dwRetBytes = bytesReturned - sizeof(USN);
        PUSN_RECORD_V2 record = (PUSN_RECORD_V2)((PBYTE)buffer + sizeof(USN));

        while (dwRetBytes > 0) {
            if (record->TimeStamp.QuadPart < bootTime.QuadPart) {
                dwRetBytes -= record->RecordLength;
                record = (PUSN_RECORD_V2)((PBYTE)record + record->RecordLength);
                continue;
            }

            std::wstring fileName((LPCWSTR)((PBYTE)record + record->FileNameOffset), record->FileNameLength / sizeof(WCHAR));
            DWORDLONG fileRef = record->FileReferenceNumber;

            if (record->Reason & USN_REASON_FILE_CREATE) {
                fileTracker[fileRef] = { record->TimeStamp, fileName };
            }

            if (record->Reason & USN_REASON_FILE_DELETE) {
                auto it = fileTracker.find(fileRef);
                if (it != fileTracker.end()) {
                    LARGE_INTEGER createTime = it->second.createTime;
                    LARGE_INTEGER deleteTime = record->TimeStamp;
                    LONGLONG timeDelta = deleteTime.QuadPart - createTime.QuadPart;

                    if (timeDelta > 100000 && timeDelta <= 9000000) {
                        size_t nameLen = it->second.fileName.length();

                        if (nameLen >= 10 && nameLen <= 30 && it->second.fileName.find(L'.') == std::wstring::npos) {
                            if (IsStrictlyAlphabetic(it->second.fileName) && HasRandomCaseRatio(it->second.fileName)) {
                                double entropy = CalculateEntropy(it->second.fileName);
                                if (entropy > 3.2) { // i tried to push this after testing it myself and afaik it does not go lower than 3.5
                                    double lifespanSecs = (double)timeDelta / 10000000.0;
                                    std::wcout << L" [!] kdmapper trace found" << std::endl;
                                    std::wcout << L" [!] Filename : " << it->second.fileName << std::endl;
                                    std::wcout << L" [!] Executed : "; PrintTime(createTime);
                                    std::wcout << L"\n [!] Lifespan : " << lifespanSecs << L" seconds\n\n";
                                }
                            }
                        }
                    }
                    fileTracker.erase(it);
                }
            }
            dwRetBytes -= record->RecordLength;
            record = (PUSN_RECORD_V2)((PBYTE)record + record->RecordLength);
        }
        readData.StartUsn = *(USN*)buffer;
    }

    CloseHandle(hVol);
}

int main() {
    ScanUSNJournal('C');
    return 0;
}
