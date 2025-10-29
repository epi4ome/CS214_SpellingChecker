#include <stdio.h> 
#include <fcntl.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <ctype.h> 
#include <dirent.h> 
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

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

/*Part 3*/
int dict_contains(const char *word) {
    return bsearch(&word, dict, wordcount, sizeof(char *), cmpstr) != NULL;
}

void report_error(const char *filename, int line, int col, const char *word) {
    if (filename) {
        dprintf(STDOUT_FILENO, "%s:%d:%d: %s\n", filename, line, col, word);
    }
    else {
        dprintf(STDOUT_FILENO, "stdin:%d:%d: %s\n", line, col, word);
    }
}

void check_word(char *word, const char *filename, int line, int col) {
    if (word == NULL || word[0] == '\0') return;
    int all_digits = 1;
    for (char *p = word; *p; p++) {
        if (!isdigit((unsigned char)*p)) { all_digits = 0; break; }
    }
    if (all_digits) return;
    if (!dict_contains(word)) {
        report_error(filename, line, col, word);
    }
}

void clean_word(char *word){
    char *start = word;
    /* skip non-alphanumeric prefix */
    while (*start && !isalnum((unsigned char)*start)) {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && !isalnum((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    memmove(word, start, end - start + 1);
    lower_case(word);
}

void scan_stream(int fd, const char *filename){
    char buffer[BUFFER];
    char word[1024];
    ssize_t bytes;
    int line = 1;
    int col = 1;
    int wlen = 0;
    int token_col = 1;

    while ((bytes = read(fd, buffer, BUFFER)) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            unsigned char c = (unsigned char)buffer[i];

            if (isalpha(c) || c == '\'' || c == '-') {
                if (wlen == 0) token_col = col;
                if (wlen < (int)sizeof(word) - 1) word[wlen++] = c;
            } else {
                if (wlen > 0) {
                    word[wlen] = '\0';
                    /* normalize and check */
                    clean_word(word);
                    if (word[0] != '\0') check_word(word, filename, line, token_col);
                    wlen = 0;
                }
                if (c == '\n') {
                    line++;
                    col = 0;
                }
            }
            col++;
        }
    }

    if (wlen > 0) {
        word[wlen] = '\0';
        clean_word(word);
        if (word[0] != '\0') check_word(word, filename, line, token_col);
    }

    if (bytes < 0) {
        perror("read");
    }
}

void scan_file(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(filename);
        return;
    }
    scan_stream(fd, filename);
    close(fd);
}
void scan_stdin(void) {
    scan_stream(STDIN_FILENO, NULL);
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
    free_dict();
    return 0;
}
