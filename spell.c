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

typedef struct {
    char *ori;
    char *lower;
} dictW;

dictW *dict = NULL;
size_t wordcount = 0;
size_t capacity = 0;
int err_found = 0;

// Create dictionary

int cmpstr(const void *a, const void *b) {
    const dictW *wa = (const dictW*)a;
    const dictW *wb = (const dictW*)b;
    return strcmp(wa->lower, wb->lower);
}

void lower_case(char *word) {
    while (*word) {
        *word = tolower((unsigned char)*word);
        word++;
    }
}

void resize_dict() {
    capacity = (capacity == 0) ? 256 : capacity * 2;
    dictW *tmp = realloc(dict, capacity * sizeof(dictW));
    if (!tmp) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    dict = tmp;
}

int check_dup(const char *lower_word) {
    for (size_t i = 0; i < wordcount; i++) {
        if (strcmp(dict[i].lower, lower_word) == 0) return 1;
    }
    return 0;
}

void add_word(const char *word) {
    char *lower = strdup(word);
    if (!lower) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    lower_case(lower);
    if (check_dup(lower)) {
        free(lower);
        return;
    }

    if (wordcount >= capacity)
        resize_dict();

    dict[wordcount].ori = strdup(word);
    dict[wordcount].lower = lower;
    if (!dict[wordcount].ori) {
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

    char buffer[BUFFER];
    char word[1024];
    size_t len = 0;
    ssize_t bytes;

    while ((bytes = read(fd, buffer, BUFFER)) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            char c = buffer[i];
            if (c != '\n') {
                if (len < sizeof(word) - 1)
                    word[len++] = c;
            } else {
                if (len > 0) {
                    word[len] = '\0';
                    add_word(word);
                    len = 0;
                }
            }
        }
    }

    if (len > 0) {
        word[len] = '\0';
        add_word(word);
    }
    close(fd);
    qsort(dict, wordcount, sizeof(dictW), cmpstr);
}

void free_dict() {
    for (size_t i = 0; i < wordcount; i++) {
        free(dict[i].ori);
        free(dict[i].lower);
    }
    free(dict);
    dict = NULL;
    wordcount = 0;
    capacity = 0;
}

// Parse arguments

void args(int argc, char **argv, char **suffix, char **dictfile, char ***paths, int *pathcount) {
    if (argc < 2) {
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

char *join_path(const char *dir, const char *file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char *res = malloc(len);
    if (!res) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(res, len, "%s/%s", dir, file);
    return res;
}

// Spelling check

int dict_contains(const char *word) {
    // FIXED: Create lowercase copy for binary search
    char lower_word[1024];
    strncpy(lower_word, word, sizeof(lower_word)-1);
    lower_word[sizeof(lower_word)-1] = '\0';
    
    // Make a copy to lowercase (don't modify original)
    for (int i = 0; lower_word[i]; i++) {
        lower_word[i] = tolower((unsigned char)lower_word[i]);
    }

    dictW key = {.lower = lower_word};
    dictW *found = bsearch(&key, dict, wordcount, sizeof(dictW), cmpstr);
    
    if (!found) return 0;

    // Now check capitalization rules
    char *dict_word = found->ori;
    if (strlen(dict_word) != strlen(word)) return 0;

    for (size_t i = 0; dict_word[i]; i++) {
        if (isupper((unsigned char)dict_word[i])) {
            // Uppercase in dict must match exactly
            if (dict_word[i] != word[i]) return 0;
        }
    }
    
    return 1;
}

void report_error(const char *filename, int line, int col, const char *word) {
    err_found = 1;
    if (filename)
        fprintf(stderr, "%s:%d:%d %s\n", filename, line, col, word);
    else
        fprintf(stderr, "stdin:%d:%d %s\n", line, col, word);
}

void check_word(char *word, const char *filename, int line, int col) {
    if (!word || !*word) return;
    
    // Skip if all digits/symbols (no letters)
    int has_letter = 0;
    for (char *p = word; *p; p++) {
        if (isalpha((unsigned char)*p)) {
            has_letter = 1;
            break;
        }
    }
    if (!has_letter) return;

    if (!dict_contains(word)) {
        report_error(filename, line, col, word);
    }
}

void scan_stream(int fd, const char *filename) {
    char buffer[BUFFER];
    char word[1024];
    ssize_t bytes;
    int line = 1, col = 1, wlen = 0, token_col = 1;

    while ((bytes = read(fd, buffer, BUFFER)) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            unsigned char c = (unsigned char)buffer[i];

            if (isspace(c)) {
                if (wlen > 0) {
                    word[wlen] = '\0';
                    
                    // Strip leading punctuation
                    char *start = word;
                    int offset = 0;
                    while (*start && strchr("([{\'\"", *start)) {
                        start++;
                        offset++;
                    }
                    
                    // Strip trailing punctuation
                    char *end = start + strlen(start);
                    while (end > start && !isalnum((unsigned char)end[-1])) {
                        end--;
                    }
                    *end = '\0';
                    
                    if (*start) {
                        check_word(start, filename, line, token_col + offset);
                    }
                    wlen = 0;
                }
                if (c == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            } else {
                if (wlen == 0) token_col = col;
                if (wlen < (int)sizeof(word) - 1)
                    word[wlen++] = c;
                col++;
            }
        }
    }

    if (wlen > 0) {
        word[wlen] = '\0';
        char *start = word;
        int offset = 0;
        while (*start && strchr("([{\'\"", *start)) {
            start++;
            offset++;
        }
        char *end = start + strlen(start);
        while (end > start && !isalnum((unsigned char)end[-1])) {
            end--;
        }
        *end = '\0';
        if (*start) {
            check_word(start, filename, line, token_col + offset);
        }
    }
}

void scan_file(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(filename);
        err_found = 1;
        return;
    }
    scan_stream(fd, filename);
    close(fd);
}

void scan_stdin(void) {
    scan_stream(STDIN_FILENO, NULL);
}

// Directory traversal 

void rec_dir(char *dirfile, char *suffix) {
    DIR *dir = opendir(dirfile);
    if (dir == NULL) {
        return;
    }
    struct dirent *current;
    while ((current = readdir(dir))) {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;
        if (current->d_name[0] == '.')
            continue;
        char *path = join_path(dirfile, current->d_name);

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
        free(path);
    }
    closedir(dir);
}

void traverse(char *path, char *suffix) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror(path);
        err_found = 1;
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        rec_dir(path, suffix);
    } else if (S_ISREG(st.st_mode)) {
        // FIXED: Don't check suffix for directly-specified files
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
    /* Debug: report how many words were loaded into the dictionary */
    fprintf(stderr, "DEBUG: dictionary words loaded = %zu\n", wordcount);
    /* Debug: list loaded dictionary entries (lowercase) */
    for (size_t i = 0; i < wordcount; i++) {
        fprintf(stderr, "DEBUG: dict[%zu] ori='%s' lower='%s'\n", i, dict[i].ori, dict[i].lower);
    }

    if (pathcount == 0)
        scan_stdin();
    else {
        for (int i = 0; i < pathcount; i++)
            traverse(paths[i], suffix);
    }

    free_dict();
    return err_found ? EXIT_FAILURE : EXIT_SUCCESS;
}