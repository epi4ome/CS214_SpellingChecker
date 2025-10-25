#include <stdio.h> 
#include <fcntl.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <ctype.h> 
#include <dirent.h> 
#include <sys/stat.h>
#include <errno.h>
#include <linux/limits.h>

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

void args(int argc, char **argv, char **suffix, char **dictfile, char ***paths, int *pathcount) {
    if (argc < 2){
        fprintf(stderr, "Error: incorrect argument format\n");
        exit(EXIT_FAILURE); 
    }
    int track = 1; 
    if (track < argc && strcmp(argv[track], "-s") == 0) {
        if (track + 1 >= argc) {
            fprintf(stderr, "Error: missing file name suffix\n");
            exit(EXIT_FAILURE);
        }
        *suffix = argv[track + 1];
        track += 2; 
    }
    if (track >= argc) {
        fprintf(stderr, "Error: missing dictionary file\n");
        exit(EXIT_FAILURE);
    }
    *dictfile = argv[track++];

    if (track < argc) {
        *paths = &argv[track];
        *pathcount = argc - track;
    }
}

int suff_check(char *name, char *suffix) {
    size_t nlen = strlen(name);
    size_t slen = strlen(suffix);
    if (slen > nlen) return 0;
    return strcmp(name + nlen - slen, suffix) == 0;
}

void rec_dir(char *dirfile,char *suffix) {
    DIR *dir = opendir(dirfile);
    if (dir == NULL){
        return; 
    }
    struct dirent *current;

    while ((current = readdir(dir))) {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirfile, current->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                rec_dir(path, suffix);
            } else if (S_ISREG(st.st_mode)) {
                if (suff_check(current->d_name, suffix)) {
                    scan_file(path);
                }
            }
        }
    }
    closedir(dir);
}

void traverse(char *path, char *suffix) {
    struct stat st;
    if (stat(path, &st) != 0){
        return; 
    }
    if (S_ISDIR(st.st_mode)){
        rec_dir(path, suffix);
    }
    else if (S_ISREG(st.st_mode) && suff_check(path, suffix)){
        scan_file(path);
    }
}

int main(int argc, char **argv) {
    char *suffix = DEFAULT; 
    char *dictfile = NULL;
    char **paths = NULL;
    int pathcount = 0;

    args(argc, argv, &suffix, &dictfile, &paths, &pathcount);
    create_dict(dictfile);

    if (pathcount == 0) {
        scan_stdin(); 
    } else {
        for (int i = 0; i < pathcount; i++) {
            traverse(paths[i], suffix);
        }
    }
    return 0;
}
