#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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
    int word_len = strlen(word);
    int count = 0;

    char* pos = buffer;

    while ((pos = strstr(pos, word)) != NULL) {
        // Check word boundaries before and after the found word
        char before = (pos == buffer) ? ' ' : *(pos - 1); // Char before 'word'
        char after = *(pos + word_len);                   // Char after 'word'

        if ((before == ' ' || before == '\t' || before == '\n' || ispunct(before)) &&
            (after == ' ' || after == '\t' || after == '\n' || after == '\0' || ispunct(after))) {
            count++;
        }

        // Move the position forward to search for the next occurrence
        pos += word_len;
    }

    data->word_count = count;
    return 0;
}

// Function to count word occurrences in a single-threaded manner
int process_file_single_thread(const char* file_path, const char* word) {
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

    int count = 0;
    char* pos = file_content;
    int word_len = strlen(word);

    while ((pos = strstr(pos, word)) != NULL) {
        // Check word boundaries before and after the found word
        char before = (pos == file_content) ? ' ' : *(pos - 1); // Char before 'word'
        char after = *(pos + word_len);                         // Char after 'word'

        if ((before == ' ' || before == '\t' || before == '\n' || ispunct(before)) &&
            (after == ' ' || after == '\t' || after == '\n' || after == '\0' || ispunct(after))) {
            count++;
        }

        // Move the position forward to search for the next occurrence
        pos += word_len;
    }

    free(file_content);
    return count;
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

    int chunk_size = file_size / MAX_THREADS;
    HANDLE threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];
    int total_word_count = 0;

    for (int i = 0; i < MAX_THREADS; ++i) {
        int start_pos = i * chunk_size;
        int end_pos = (i == MAX_THREADS - 1) ? file_size : (i + 1) * chunk_size;

        // Adjust to word boundaries
        if (i > 0) { // For all but the first thread
            while (start_pos < file_size && !isspace(file_content[start_pos])) {
                start_pos++;
            }
        }
        if (i < MAX_THREADS - 1) { // For all but the last thread
            while (end_pos > 0 && !isspace(file_content[end_pos - 1])) {
                end_pos--;
            }
        }

        int actual_chunk_size = end_pos - start_pos;
        if (actual_chunk_size > 0) {
            thread_data[i].buffer = (char*)malloc(actual_chunk_size + 1);
            strncpy(thread_data[i].buffer, file_content + start_pos, actual_chunk_size);
            thread_data[i].buffer[actual_chunk_size] = '\0';
        } else {
            thread_data[i].buffer = NULL; // Handle empty chunk
        }

        thread_data[i].word = (char*)word;
        thread_data[i].word_count = 0;

        threads[i] = CreateThread(NULL, 0, count_word_in_chunk, &thread_data[i], 0, NULL);
        if (threads[i] == NULL) {
            printf("Error creating thread %d\n", i);
            free(file_content);
            return -1;
        }
    }

    WaitForMultipleObjects(MAX_THREADS, threads, TRUE, INFINITE);

    for (int i = 0; i < MAX_THREADS; ++i) {
        total_word_count += thread_data[i].word_count;
        free(thread_data[i].buffer);
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

// Function to get the elapsed time
double get_time(LARGE_INTEGER *start, LARGE_INTEGER *end) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (double)(end->QuadPart - start->QuadPart) / frequency.QuadPart;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <directory> <word>\n", argv[0]);
        return 1;
    }

    const char* directory = argv[1];
    const char* word = argv[2];

    // Variables to hold start and end times
    LARGE_INTEGER start, end;
    double single_thread_time, multi_thread_time;

    // Process each file in the directory for timing analysis
    WIN32_FIND_DATA find_file_data;
    HANDLE hFind;
    char search_path[BUFFER_SIZE];

    snprintf(search_path, BUFFER_SIZE, "%s\\*", directory);

    hFind = FindFirstFile(search_path, &find_file_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("FindFirstFile failed (%d)\n", GetLastError());
        return 1;
    }

    do {
        if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char file_path[BUFFER_SIZE];
            snprintf(file_path, BUFFER_SIZE, "%s\\%s", directory, find_file_data.cFileName);

            printf("\nProcessing file: %s\n", file_path);

            // Measure single-threaded time
            QueryPerformanceCounter(&start);
            int single_thread_count = process_file_single_thread(file_path, word);
            QueryPerformanceCounter(&end);
            single_thread_time = get_time(&start, &end);
            printf("Single-threaded count: %d, Time: %.6f seconds\n", single_thread_count, single_thread_time);

            // Measure multi-threaded time
            QueryPerformanceCounter(&start);
            int multi_thread_count = process_file_in_threads(file_path, word);
            QueryPerformanceCounter(&end);
            multi_thread_time = get_time(&start, &end);
            printf("Multi-threaded count: %d, Time: %.6f seconds\n", multi_thread_count, multi_thread_time);

            // Verify that both methods yield the same result
            if (single_thread_count != multi_thread_count) {
                printf("Warning: Count mismatch between single-threaded and multi-threaded!\n");
            }
        }
    } while (FindNextFile(hFind, &find_file_data) != 0);

    FindClose(hFind);

    return 0;
}