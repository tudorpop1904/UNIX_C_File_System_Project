#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#undef FILE_MAX
#ifndef FILE_MAX
#define FILE_MAX 65536
#endif
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#undef MAX_BUF_SIZE
#ifndef MAX_BUF_SIZE
#define MAX_BUF_SIZE 1000000
#endif

off_t fileSize(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
    	perror("Can't Open File!");
    	return 0;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    close(fd);
    return size;
}

void list(const char *path, int isRecursive, off_t smaller, int permWrite) {
    DIR *dir = opendir(path);
    struct dirent *entry = NULL;
    struct stat statBuf;
    char newPath[PATH_MAX];
    while ((entry = readdir(dir)) != NULL) {
    	if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
    	    snprintf(newPath, PATH_MAX, "%s/%s", path, entry->d_name);
    	    if (lstat(newPath, &statBuf) == 0) {
    	    	if (permWrite) {
    	    	    if ((statBuf.st_mode & S_IWUSR) != 0) {
    	    	    	if (S_ISDIR(statBuf.st_mode)) {
    	    	    	    if (smaller == __INT32_MAX__)
    	    	    	        printf("%s\n", newPath);
    	    	    	    if (isRecursive)
    	    	    	    	list(newPath, isRecursive, smaller, permWrite);
    	    	    	}
    	    	    	else if (fileSize(newPath) < smaller)
    	    	    	    printf("%s\n", newPath);
    	    	    }
    	    	}
    	    	else {
    	    	    if (S_ISDIR(statBuf.st_mode)) {
    	    	        if (smaller == __INT32_MAX__)
    	    	            printf("%s\n", newPath);
    	    	    	if (isRecursive)
    	    	    	    list(newPath, isRecursive, smaller, permWrite);
    	    	    }
    	    	    else if (fileSize(newPath) < smaller)
    	    	    	printf("%s\n", newPath);
    	    	}
    	    }
    	}
    }
    closedir(dir);
}

typedef struct {
    char *MAGIC;
    unsigned short HEADER_SIZE;
    unsigned char VERSION;
    unsigned char NO_OF_SECTIONS;
    char SECTION_NAME[18][11];
    unsigned char SECTION_TYPE[18];
    unsigned int SECTION_OFFSET[18];
    unsigned int SECTION_SIZE[18];
}SF;

char *parse(const char *path, SF *parsed, char buf[]) {
    strcpy(buf, "");
    int fd = open(path, O_RDONLY);
    int cnt = 0;
    if (fd < 0) {
        perror("Can't Open File!");
        return "Can't Open File!";
    }
    read(fd, buf, 1);
    cnt++;
    if (strcmp(buf, "i") != 0) {
        close(fd);
        return "ERROR\nwrong magic\n";
    }
    strcpy(parsed->MAGIC, buf);
    read(fd, buf, 2);
    parsed->HEADER_SIZE = *((unsigned int *)(buf + cnt));
    cnt += 2;
    read(fd, buf + cnt, 1);
    unsigned char version = buf[cnt];
    if (version < 30 || version > 96) {
        close(fd);
        return "ERROR\nwrong version\n";
    }
    parsed->VERSION = version;
    cnt++;
    read(fd, buf + cnt, 1);
    unsigned char numSections = buf[cnt];
    if ((numSections != 2) && (numSections < 4 || numSections > 18)) {
        close(fd);
        return "ERROR\nwrong sect_nr\n";
    }
    parsed->NO_OF_SECTIONS = numSections;
    cnt++;
    char *sectNames[numSections];
    int sectTypes[numSections];
    unsigned int sectSizes[numSections];
    for (int i = 0; i < numSections; i++) {
        read(fd, buf + cnt, 11);
        sectNames[i] = malloc(11);
        strncpy(sectNames[i], buf + cnt, 11);
        strcpy(parsed->SECTION_NAME[i], sectNames[i]);
        cnt += 11;
        read(fd, buf + cnt, 1);
        sectTypes[i] = buf[cnt];
        if (sectTypes[i] != 42 && sectTypes[i] != 37 && sectTypes[i] != 55) {
            close(fd);
            return "ERROR\nwrong sect_types\n";
        }
        parsed->SECTION_TYPE[i] = sectTypes[i];
        cnt++;
        read(fd, buf + cnt, 4);
        parsed->SECTION_OFFSET[i] = *((unsigned int *)(buf + cnt));
        cnt += 4;
        read(fd, buf + cnt, 4);
        sectSizes[i] = *((unsigned int *)(buf + cnt));
        parsed->SECTION_SIZE[i] = sectSizes[i];
        cnt += 4;
    }
    char *success = (char *)malloc(FILE_MAX);
    snprintf(success, FILE_MAX, "SUCCESS\nversion=%d\nnr_sections=%d\n", version, numSections);
    int currSize = strlen(success);
    for (int i = 0; i < numSections; i ++) {
        snprintf(success + currSize, FILE_MAX, "section%d: %s %d %d\n", i + 1, sectNames[i], sectTypes[i], sectSizes[i]);
        currSize = strlen(success);
        free(sectNames[i]);
    }
    close(fd);
    return success;
}

void strrev(char str[]) {
    size_t len = strlen(str);
    char *start = str;
    char *end = str + len - 1;
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start ++;
        end --;
    }
}

char *extract(const char *path, int section, int line, SF *parsed, char buf[]) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return "ERROR\ninvalid file\n";
    off_t sectionOffset = parsed->SECTION_OFFSET[section - 1];
    size_t sectionSize = parsed->SECTION_SIZE[section - 1];
    lseek(fd, sectionOffset, SEEK_SET);
    ssize_t bytesRead = read(fd, buf, sectionSize);
    close(fd);
    if (bytesRead != sectionSize)
        return "ERROR\nunable to read section\n";
    buf[sectionSize] = '\0';  // Null-terminate the buffer
    // Reverse the buffer
    size_t i, j;
    for (i = 0, j = strlen(buf) - 1; i < j; ++i, --j) {
        char temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
    }
    // Count lines and check if the requested line is valid
    int lineCount = 0;
    char *linePtr = buf;
    while ((linePtr = strchr(linePtr, '\n')) != NULL) {
        lineCount++;
        linePtr++;  // Move past the newline character
    }
    if (line < 1 || line > lineCount)
        return "ERROR\ninvalid line\n";
    // Extract the requested line
    char *startPos = buf;
    while (--line && (startPos = strchr(startPos, '\n')) != NULL) {
        startPos++;  // Move to the next line
    }
    // Find the end of the line
    char *endPos = strchr(startPos, '\n');
    if (endPos == NULL) {
        // Last line
        endPos = buf + strlen(buf);
    }

    // Allocate memory for the extracted line
    size_t lineLen = endPos - startPos;
    char *extractedLine = malloc(lineLen + 1);
    if (extractedLine == NULL)
        return "ERROR\nmemory allocation failed\n";
    strncpy(extractedLine, startPos, lineLen);
    extractedLine[lineLen] = '\0';  // Null-terminate the extracted line
    char *finalString = malloc(strlen(extractedLine) + strlen("SUCCESS\n"));
    strcpy(finalString, "SUCCESS\n");
    strcat(finalString, extractedLine);
    free(extractedLine);
    return finalString;
}

void findall(const char *path, SF *parsed) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    struct dirent *entry = NULL;
    char newPath[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            snprintf(newPath, PATH_MAX, "%s/%s", path, entry->d_name);
            struct stat statBuf;
            if (lstat(newPath, &statBuf) == 0) {
                if (S_ISDIR(statBuf.st_mode)) {
                    findall(newPath, parsed);
                } else {
                    char buf[FILE_MAX] = "";
                    if (strstr(parse(newPath, parsed, buf), "ERROR\n") == NULL) {
                        int sectionExceedsSize = 0;
                        for (int i = 0; i < parsed->NO_OF_SECTIONS; i++) {
                            if (parsed->SECTION_SIZE[i] > 1211) {
                                sectionExceedsSize = 1;
                                break;
                            }
                        }
                        if (!sectionExceedsSize) {
                            printf("%s\n", newPath);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
    	printf("Usages:\n./a1 variant\n./a1 list [recursive] <filtering_options> path=<dir_path>\n./a1 parse path=<file_path>\n./a1 extract path=<file_path> section=<sect_nr> line=<line_nr>\n./a1 findall path=<dir_path>\n");
    	return -1;
    }
    if (argc == 2) {
    	if (strcmp(argv[1], "variant") == 0) {
    	    printf("18354\n");
    	    return 0;
    	}
    	else {
    	    printf("Usage: ./a1 variant");
    	    return -1;
    	}
    }
    if (argc >= 3) {
    	char path[PATH_MAX] = "";
    	if (strcmp(argv[1], "list") == 0) {
    	    int isRecursive = 0;
    	    int permWrite = 0;
    	    int smaller = __INT32_MAX__;
    	    for (int i = 2; i < argc; i ++) {
    	    	if (strcmp(argv[i], "recursive") == 0)
    	    	    isRecursive = 1;
    	    	if (strcmp(argv[i], "has_perm_write") == 0)
    	    	    permWrite = 1;
    	    	if (strstr(argv[i], "size_smaller=") != NULL)
    	    	    sscanf(argv[i], "size_smaller=%d", &smaller);
    	    	if (strstr(argv[i], "path=") != NULL)
    	    	    snprintf(path, PATH_MAX, "%s", argv[i] + 5);
    	    }
    	    if (strcmp(path, "") != 0) {
    	    	DIR *dir = opendir(path);
    	    	if (dir == NULL) {
    	    	    printf("ERROR\ninvalid directory path\n");
    	    	    return -1;
    	    	}
    	    	else {
    	    	    closedir(dir);
    	    	    printf("SUCCESS\n");
    	    	    list(path, isRecursive, smaller, permWrite);
    	    	    return 0;
    	    	}
    	    }
    	    else {
    	    	printf("ERROR\ninvalid directory path\n");
    	    	return -1;
    	    }
    	}
    	else if (strcmp(argv[1], "parse") == 0 || strcmp(argv[2], "parse") == 0) {
    	    if (strcmp(argv[1], "parse") == 0) {
    	        if (strstr(argv[2], "path=") != NULL)
    	    	    sscanf(argv[2], "path=%s", path);
    	    	else {
    	    	    printf("Usage: ./a1 parse path=<file_path>\n./a1 path=<file_path> parse\n");
    	    	    return -1;
    	    	}
    	    }
    	    else if (strcmp(argv[2], "parse") == 0) {
    	        if (strstr(argv[1], "path=") != NULL)
    	    	    sscanf(argv[1], "path=%s", path);
    	    	else {
    	    	    printf("Usage: ./a1 parse path=<file_path>\n./a1 path=<file_path> parse\n");
    	    	    return -1;
    	    	}
    	    }
    	    if (strcmp(path, "") != 0) {
    	 	char buf[FILE_MAX] = "";
    	 	SF *parseThis = (SF*)malloc(sizeof(SF));
    	 	parseThis->MAGIC = (char *)malloc(1);
    	 	printf("%s", parse(path, parseThis, buf));
    	 	free(parseThis->MAGIC);
    	 	free(parseThis);
    	 	return 0;
    	    }
    	}
    	else if (strcmp(argv[1], "extract") == 0) {
    	    char buf[FILE_MAX] = "";
    	    SF *parseThis = (SF *)malloc(sizeof(SF));
    	    int section = INT32_MIN, line = INT32_MIN;
    	    parseThis->MAGIC = (char *)malloc(1);
    	    if (strstr(argv[2], "path=") != NULL)
    	    	sscanf(argv[2], "path=%s", path);
    	    else {
    	    	printf("Usage: ./a1 extract path=<file_path> section=<sect_nr> line=<line_nr>\n");
    	 	free(parseThis->MAGIC);
    	 	free(parseThis);
    	    	return -1;
    	    }
    	    if (strstr(argv[3], "section=") != NULL)
    	    	sscanf(argv[3], "section=%d", &section);
    	    else {
    	    	printf("Usage: ./a1 extract path=<file_path> section=<sect_nr> line=<line_nr>\n");
    	 	free(parseThis->MAGIC);
    	 	free(parseThis);
    	    	return -1;
    	    }
    	    if (strstr(argv[4], "line=") != NULL)
    	    	sscanf(argv[4], "line=%d", &line);
    	    else {
    	    	printf("Usage: ./a1 extract path=<file_path> section=<sect_nr> line=<line_nr>\n");
    	 	free(parseThis->MAGIC);
    	 	free(parseThis);
    	    	return -1;
    	    }
    	    if (strcmp(path, "") != 0) {
    	    	if (strstr(parse(path, parseThis, buf), "ERROR\nwrong ") == NULL) {
    	    	    printf("%s", extract(path, section, line, parseThis, buf));
    	    	    return 0;
    	    	}
    	    }
    	}
    	else if (strcmp(argv[1], "findall") == 0) {
    	    if (strstr(argv[2], "path=") != NULL)
    	    	sscanf(argv[2], "path=%s", path);
    	    DIR *dir = opendir(path);
    	    if (dir == NULL) {
    	    	printf("ERROR\ninvalid directory path\n");
    	    	return -1;
    	    }
    	    else {
    	    	closedir(dir);
    	    	printf("SUCCESS\n");
    	    	SF *parseThis = (SF *)malloc(sizeof(SF));
    	    	parseThis->MAGIC = (char *)malloc(1);
    	    	findall(path, parseThis);
    	    	free(parseThis->MAGIC);
    	    	free(parseThis);
    	    	return 0;
    	    }
    	}
    }
    return 0;
}
