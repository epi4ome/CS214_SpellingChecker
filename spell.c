#include <stdio.h> 
#include <fcntl.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <ctype.h> 
#include <dirent.h> 
#include <errno.h>

#define BUFFER 128
#define DEFAULT ".txt"

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
            exit(EXIT_FAILURE);
        }
        dict = tmp;
    }

    dict[wordcount] = strdup(word);
    if (!dict[wordcount]) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    wordcount++;
}

void create_dict(char *file) {
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror(file);
        exit(EXIT_FAILURE);
    }

    size_t capacity = 256;
    dict = malloc(capacity * sizeof(char *));
    if (dict == NULL) {
        perror("malloc");
        close(fd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER];
    char word[1024];
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
        exit(EXIT_FAILURE);
    }
    if (len > 0) {
        word[len] = '\0';
        lower_case(word);
        add_word(word, &capacity); 
    }

    close(fd);
    qsort(dict, wordcount, sizeof(char *), cmpstr);
}

void free_dict(void) {
    for (size_t i = 0; i < wordcount; i++){
        free(dict[i]);
    }
    free(dict);
    dict = NULL;
}

int main(int argc, char **argv) {
    char *suffix = DEFAULT; 
    char *dictfile = NULL;
    char **paths = NULL;
    int pathcount = 0;

    parse_args(argc, argv, &suffix, &dictfile, &paths, &pathcount);
    create_dict(dictfile);

    return 0;
}
