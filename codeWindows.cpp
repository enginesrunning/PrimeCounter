#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include <thread>

struct ProcessInfo {
    HANDLE processHandle;
    HANDLE pipeRead;
};

bool isPrime(int num) {
    if (num <= 1) return false;
    for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) return false;
    }
    return true;
}

void ProcessRange(int start, int end) {
    for (int i = start; i <= end; i++) {
        if (isPrime(i)) {
            std::cout << "Process " << (start/1000) << " found prime: " << i << std::endl;
        }
    }
}

// Function to read from pipe and output to console
void ReadFromPipe(HANDLE pipe) {
    char buffer[4096];
    DWORD bytesRead;

    while (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::cout << buffer;
    }
}

int main(int argc, char* argv[]) {
    const int NUM_PROCESSES = 10;
    const int RANGE_SIZE = 1000;

    // If this is a child process
    if (argc == 3) {
        int start = atoi(argv[1]);
        int end = atoi(argv[2]);
        ProcessRange(start, end);
        return 0;
    }

    // Parent process code
    std::vector<ProcessInfo> processes;
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    for (int i = 0; i < NUM_PROCESSES; i++) {
        // Create pipe for redirecting output
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        HANDLE pipeRead, pipeWrite;
        if (!CreatePipe(&pipeRead, &pipeWrite, &saAttr, 0)) {
            std::cerr << "CreatePipe failed. Error: " << GetLastError() << std::endl;
            return 1;
        }

        // Ensure the read handle is not inherited
        SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, 0);

        // Create command line for child process
        std::string cmdLine = std::string(exePath) + " " +
                            std::to_string(i * RANGE_SIZE + 1) + " " +
                            std::to_string((i + 1) * RANGE_SIZE);

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdOutput = pipeWrite;
        si.hStdError = pipeWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;
        ZeroMemory(&pi, sizeof(pi));

        // Create child process
        if (!CreateProcess(
            NULL,                                   // No module name (use command line)
            const_cast<LPSTR>(cmdLine.c_str()),    // Command line
            NULL,                                   // Process handle not inheritable
            NULL,                                   // Thread handle not inheritable
            TRUE,                                   // Set handle inheritance to TRUE
            0,                                      // No creation flags
            NULL,                                   // Use parent's environment block
            NULL,                                   // Use parent's starting directory
            &si,                                    // Pointer to STARTUPINFO structure
            &pi                                     // Pointer to PROCESS_INFORMATION structure
        )) {
            std::cerr << "CreateProcess failed. Error: " << GetLastError() << std::endl;
            CloseHandle(pipeRead);
            CloseHandle(pipeWrite);
            return 1;
        }

        // Close unnecessary handles
        CloseHandle(pi.hThread);
        CloseHandle(pipeWrite);

        // Store process info
        ProcessInfo processInfo = {pi.hProcess, pipeRead};
        processes.push_back(processInfo);

        // Create thread to read from pipe
        std::thread(ReadFromPipe, pipeRead).detach();
    }

    // Wait for all child processes to complete
    std::vector<HANDLE> processHandles;
    for (const auto& process : processes) {
        processHandles.push_back(process.processHandle);
    }
    WaitForMultipleObjects(NUM_PROCESSES, processHandles.data(), TRUE, INFINITE);

    // Clean up handles
    for (const auto& process : processes) {
        CloseHandle(process.processHandle);
        CloseHandle(process.pipeRead);
    }

    return 0;
}
