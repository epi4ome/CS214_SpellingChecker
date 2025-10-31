#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *dictfile = "dict.txt";
    const char *testfile = "test_all.txt";
    const char *expectedfile = "expected_output.txt";
    const char *outfile = "output.txt";

    char command[256];
    snprintf(command, sizeof(command), "./spell.o %s %s > %s", dictfile, testfile, outfile);
    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "Error: spell command failed (exit %d)\n", ret);
        return 1;
    }

    FILE *out = fopen(outfile, "r");
    FILE *exp = fopen(expectedfile, "r");
    if (!out || !exp) {
        perror("fopen");
        return 1;
    }

    char line_out[256], line_exp[256];
    int line = 1, failed = 0;

    while (fgets(line_exp, sizeof(line_exp), exp)) {
        if (!fgets(line_out, sizeof(line_out), out)) {
            printf("❌ FAIL: Missing output after line %d (expected \"%s\")\n",
                   line, line_exp);
            failed = 1;
            break;
        }

        line_exp[strcspn(line_exp, "\n")] = '\0';
        line_out[strcspn(line_out, "\n")] = '\0';

        if (strcmp(line_out, line_exp) != 0) {
            printf("❌ FAIL at line %d:\n", line);
            printf("   Expected: \"%s\"\n", line_exp);
            printf("   Got:      \"%s\"\n", line_out);
            failed = 1;
        }
        line++;
    }

    if (fgets(line_out, sizeof(line_out), out)) {
        printf("❌ FAIL: Extra unexpected output: \"%s\"\n", line_out);
        failed = 1;
    }

    fclose(out);
    fclose(exp);

    if (!failed) {
        printf("✅ PASS: All output matched expected results!\n");
    }

    return failed;
}
