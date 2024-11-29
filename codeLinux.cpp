#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstring>

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
void ReadFromPipe(int pipefd) {
    char buffer[4096];
    ssize_t bytesRead;
    
    while ((bytesRead = read(pipefd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        std::cout << buffer << std::flush;
    }
}

struct ProcessInfo {
    pid_t pid;
    int pipefd;
};

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
    std::string execPath = argv[0];

    for (int i = 0; i < NUM_PROCESSES; i++) {
        // Create pipe
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            std::cerr << "Pipe creation failed: " << strerror(errno) << std::endl;
            return 1;
        }

        // Make read end non-blocking
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

        pid_t pid = fork();
        
        if (pid == -1) {
            std::cerr << "Fork failed: " << strerror(errno) << std::endl;
            return 1;
        }

        if (pid == 0) {  // Child process
            // Close read end
            close(pipefd[0]);

            // Redirect stdout and stderr to pipe
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            // Convert numbers to strings for exec
            std::string start = std::to_string(i * RANGE_SIZE + 1);
            std::string end = std::to_string((i + 1) * RANGE_SIZE);

            // Execute the program with new arguments
            execl(execPath.c_str(), execPath.c_str(), start.c_str(), end.c_str(), nullptr);
            
            // If execl fails
            std::cerr << "Exec failed: " << strerror(errno) << std::endl;
            exit(1);
        }
        else {  // Parent process
            // Close write end
            close(pipefd[1]);

            // Store process info
            ProcessInfo processInfo = {pid, pipefd[0]};
            processes.push_back(processInfo);

            // Create thread to read from pipe
            std::thread([pipefd = pipefd[0]]() {
                ReadFromPipe(pipefd);
            }).detach();
        }
    }

    // Wait for all child processes to complete
    for (const auto& process : processes) {
        int status;
        waitpid(process.pid, &status, 0);
    }

    // Clean up file descriptors
    for (const auto& process : processes) {
        close(process.pipefd);
    }

    return 0;
}