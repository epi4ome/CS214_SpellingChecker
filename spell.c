#include <stdio.h> 
#include <fcntl.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <ctype.h> 
#include <dirent.h> 
#include <errno.h>

#define BUFFER 128
char **dict = NULL;
size_t wordcount = 0; 

int cmpstr(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

void lower_case(char *word) {
    while (*word != '\0') {
        *word = tolower(*word);
        word++;
    }
}

void add_word(const char *word, size_t *capacity) {
    if (wordcount >= *capacity) {
        *capacity *= 2;
        char **tmp = realloc(dict, *capacity * sizeof(char *));
        if (!tmp) {
            perror("realloc");
            exit(2);
        }
        dict = tmp;
    }

    dict[wordcount] = strdup(word);
    if (!dict[wordcount]) {
        perror("strdup");
        exit(2);
    }
    wordcount++;
}

void create_dict(char *file) {
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror(file);
        exit(2);
    }

    size_t capacity = 256;
    dict = malloc(capacity * sizeof(char *));
    if (dict == NULL) {
        perror("malloc");
        close(fd);
        exit(2);
    }

    char buffer[BUFFER];
    char word[128];
    size_t len = 0;
    int bytes;

   while ((bytes = read(fd, buffer, BUFFER)) > 0) {
        for (int i = 0; i < bytes; i++) {
            char c = buffer[i];
            if (c != '\n') {
                if (len < sizeof(word) - 1) {
                    word[len++] = c;
                }
            } 
            else {
                if (len > 0) {
                    word[len] = '\0';
                    lower_case(word);
                    add_word(word, &capacity); 
                    len = 0;
                }
            }
        }
    }
    if (bytes < 0) {
        perror("read");
        close(fd);
        exit(2);
    }
    if (len > 0) {
        word[len] = '\0';
        lower_case(word);
        add_word(word, &capacity); 
    }

    close(fd);
    qsort(dict, wordcount, sizeof(char *), cmpstr);
}



