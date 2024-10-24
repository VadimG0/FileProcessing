#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_THREADS 4
#define BUFFER_SIZE 1024
#define PIPE_NAME "\\\\.\\pipe\\WordCountPipe"

// Structure to pass data to each thread
typedef struct {
    char* buffer;
    char* word;
    int word_count;
} ThreadData;

// Function to count word occurrences in a given buffer
DWORD WINAPI count_word_in_chunk(LPVOID param) {
    ThreadData* data = (ThreadData*)param;
    char* buffer = data->buffer;
    char* word = data->word;
    int count = 0;

    // Tokenize the buffer and count occurrences of the word
    char* token = strtok(buffer, " \t\n");
    while (token != NULL) {
        if (strcmp(token, word) == 0) {
            count++;
        }
        token = strtok(NULL, " \t\n");
    }

    data->word_count = count;
    return 0;
}

// Function to process a file by creating multiple threads
int process_file_in_threads(const char* file_path, const char* word) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", file_path);
        return -1;
    }

    // Read the file into memory
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_content = (char*)malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Divide file content into chunks for each thread
    int chunk_size = file_size / MAX_THREADS;
    HANDLE threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];
    int total_word_count = 0;

    for (int i = 0; i < MAX_THREADS; ++i) {
        thread_data[i].buffer = file_content + i * chunk_size;
        thread_data[i].word = (char*)word;
        thread_data[i].word_count = 0;

        // Create thread to count words in each chunk
        threads[i] = CreateThread(NULL, 0, count_word_in_chunk, &thread_data[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("Error creating thread %d\n", i);
            return -1;
        }
    }

    // Wait for all threads to complete
    WaitForMultipleObjects(MAX_THREADS, threads, TRUE, INFINITE);

    // Collect word counts from each thread
    for (int i = 0; i < MAX_THREADS; ++i) {
        total_word_count += thread_data[i].word_count;
    }

    free(file_content);
    return total_word_count;
}

// Function to create a child process for each file
void create_child_process(const char* file_path, const char* word) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char command[BUFFER_SIZE];

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Prepare the command to execute (passing file and word as arguments)
    snprintf(command, BUFFER_SIZE, "ChildProcess.exe %s %s", file_path, word);

    // Create a child process
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return;
    }

    // Wait for the child process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// Function to process each file in the directory
void process_directory(const char* directory, const char* word) {
    WIN32_FIND_DATA find_file_data;
    HANDLE hFind;
    char search_path[BUFFER_SIZE];

    // Create the search path (directory\*)
    snprintf(search_path, BUFFER_SIZE, "%s\\*", directory);

    // Start searching for files in the directory
    hFind = FindFirstFile(search_path, &find_file_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("FindFirstFile failed (%d)\n", GetLastError());
        return;
    }

    do {
        // Skip directories (we are only interested in regular files)
        if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Create the full file path
            char file_path[BUFFER_SIZE];
            snprintf(file_path, BUFFER_SIZE, "%s\\%s", directory, find_file_data.cFileName);

            printf("Processing file: %s\n", file_path);

            // Create a child process to handle the file
            create_child_process(file_path, word);
        }
    } while (FindNextFile(hFind, &find_file_data) != 0);

    // Close the handle when finished
    FindClose(hFind);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <directory> <word>\n", argv[0]);
        return 1;
    }

    const char* directory = argv[1];
    const char* word = argv[2];

    // Process each file in the directory
    process_directory(directory, word);

    return 0;
}
