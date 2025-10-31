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
#define OPENING_PUNCTUATION "([{\'\""

typedef struct {
    char *ori;
    char *lower;
} dictW;

dictW *dict = NULL;
size_t wordcount = 0;
size_t capacity = 0;
int err_found = 0;

// Dictionary functions

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
    if (capacity == 0) {
        capacity = 256;
    } else {
        capacity = capacity * 2;
    }
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
    size_t len = strlen(word);
    
    char *lower = malloc(len + 1);
    if (!lower) {
        perror("malloc for lower");
        exit(EXIT_FAILURE);
    }
    strcpy(lower, word); 
    
    lower_case(lower);
    if (check_dup(lower)) {
        free(lower);
        return;
    }

    if (wordcount >= capacity)
        resize_dict();

    char *ori = malloc(len + 1);
    if (!ori) {
        perror("malloc for ori");
        free(lower);
        exit(EXIT_FAILURE);
    }
    strcpy(ori, word);

    dict[wordcount].ori = ori;
    dict[wordcount].lower = lower;
    
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
            
            if (c == '\n') {
                if (len > 0) {
                    word[len] = '\0';
                    add_word(word);
                }
                len = 0;
            } else {
                if (len < sizeof(word) - 1)
                    word[len++] = c;
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

// Argument Parsing

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

// Path Management

int suff_check(char *name, char *suffix) {
    size_t nlen = strlen(name);
    size_t slen = strlen(suffix);
    if (slen > nlen) return 0;
    return strcmp(name + nlen - slen, suffix) == 0;
}

int ends_with_suffix(const char *path, const char *suffix) {
    size_t path_len = strlen(path);
    size_t suff_len = strlen(suffix);
    if (suff_len > path_len) return 0;
    return strcmp(path + path_len - suff_len, suffix) == 0;
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

int check_capitalization(const char *input_word, const char *dict_word) {
    size_t len = strlen(input_word);
    if (len != strlen(dict_word)) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (isupper((unsigned char)dict_word[i])) {
            if (dict_word[i] != input_word[i]) {
                return 0;
            }
        }
        else if (tolower((unsigned char)dict_word[i]) != tolower((unsigned char)input_word[i])) {
             return 0;
        }
    }
    return 1;
}

int dict_contains(const char *word) {
    char lower_word[1024];
    strncpy(lower_word, word, sizeof(lower_word)-1);
    lower_word[sizeof(lower_word)-1] = '\0';
    
    for (int i = 0; lower_word[i]; i++) {
        lower_word[i] = tolower((unsigned char)lower_word[i]);
    }
    dictW key = {.lower = lower_word};
    dictW *found = bsearch(&key, dict, wordcount, sizeof(dictW), cmpstr);
    
    if (!found) {
        return 0;
    }

    return check_capitalization(word, found->ori);
}

void report_error(const char *filename, int line, int col, const char *word) {
    err_found = 1;
    if (filename)
        fprintf(stderr, "%s:%d:%d %s\n", filename, line, col, word);
    else
        fprintf(stderr, "stdin:%d:%d %s\n", line, col, word);
}

void check_word(char *word, const char *filename, int line, int col) {
    if (!word || !*word) {
        return;
    }

    int has_letter = 0;
    for (char *p = word; *p; p++) {
        if (isalpha((unsigned char)*p)) {
            has_letter = 1;
            break;
        }
    }
    if (!has_letter) {
        return;
    }

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
                
                    char *start = word;
                    int offset = 0;
                    while (*start && strchr(OPENING_PUNCTUATION, *start)) {
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
                    wlen = 0;
                }
                
                if (c == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            } else {
                if (wlen == 0) {
                    token_col = col; 
                }
                if (wlen < (int)sizeof(word) - 1) {
                    word[wlen++] = c;
                }
                col++;
            }
        }
    }

    if (wlen > 0) {
        word[wlen] = '\0';
        char *start = word;
        int offset = 0;
        while (*start && strchr(OPENING_PUNCTUATION, *start)) {
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

// Directory Traversal 

void rec_dir(char *dirfile, char *suffix) {
    DIR *dir = opendir(dirfile);
    if (dir == NULL) {
        return;
    }
    struct dirent *current;
    while ((current = readdir(dir))) {
        if (current->d_name[0] == '.') {
            continue;
        }
            
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
    const char *final_path = path;
    char *modified_path = NULL; 
    int stat_res = stat(path, &st);

    if (stat_res != 0 && !ends_with_suffix(path, suffix)) {
        size_t new_len = strlen(path) + strlen(suffix) + 1;
        
        modified_path = malloc(new_len);
        if (modified_path == NULL) {
            perror("malloc");
            err_found = 1;
            return;
        }
        if (snprintf(modified_path, new_len, "%s%s", path, suffix) < 0) {
            free(modified_path);
            modified_path = NULL;
            fprintf(stderr, "Error constructing path: %s\n", path);
            err_found = 1;
            return;
        }
        
        stat_res = stat(modified_path, &st);
        
        if (stat_res == 0) {
            final_path = modified_path; 
        }
    }

    if (stat_res != 0) {
        perror(path); 
        err_found = 1;
    } else if (S_ISDIR(st.st_mode)) {
        rec_dir((char*)final_path, suffix);
    } else if (S_ISREG(st.st_mode)) {
        scan_file(final_path);
    }
    if (modified_path != NULL) {
        free(modified_path);
    }
}

int main(int argc, char **argv) {
    char *suffix = DEFAULT;
    char *dictfile = NULL;
    char **paths = NULL;
    int pathcount = 0;

    args(argc, argv, &suffix, &dictfile, &paths, &pathcount);
    create_dict(dictfile);
    
    fprintf(stderr, "DEBUG: dictionary words loaded = %zu\n", wordcount);
    for (size_t i = 0; i < wordcount; i++) {
        fprintf(stderr, "DEBUG: dict[%zu] ori='%s' lower='%s'\n", i, dict[i].ori, dict[i].lower);
    }

    if (pathcount == 0) {
        scan_stdin();
    } else {
        for (int i = 0; i < pathcount; i++) {
            traverse(paths[i], suffix);
        }
    }

    free_dict();
    
    if (err_found) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
