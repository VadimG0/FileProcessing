#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int count_word_in_file(const char* file_path, const char* word) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", file_path);
        return -1;
    }

    int count = 0;
    char buffer[1024];

    // Read the file line by line and count occurrences of the word
    while (fgets(buffer, sizeof(buffer), file)) {
        char* token = strtok(buffer, " \t\n");
        while (token != NULL) {
            if (strcmp(token, word) == 0) {
                count++;
            }
            token = strtok(NULL, " \t\n");
        }
    }

    fclose(file);
    return count;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: ChildProcess <file_path> <word>\n");
        return 1;
    }

    const char* file_path = argv[1];
    const char* word = argv[2];
    int word_count = count_word_in_file(file_path, word);

    printf("File: %s, Word: '%s', Count: %d\n", file_path, word, word_count);
    return 0;
}
