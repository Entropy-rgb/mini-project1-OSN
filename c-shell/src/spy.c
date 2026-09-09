#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "spy.h"

static const char* get_type_str(mode_t mode) {
    if (S_ISREG(mode)) return "REG";
    if (S_ISDIR(mode)) return "DIR";
    if (S_ISCHR(mode)) return "CHR";
    if (S_ISBLK(mode)) return "BLK";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISSOCK(mode)) return "SOCK";
    if (S_ISLNK(mode)) return "LNK";
    return "UNK";
}

static void print_entry(const char *pid, const char *fd, const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("%-8s %-6s %-6s %s\n", pid, fd, get_type_str(st.st_mode), path);
    } else if (lstat(path, &st) == 0) {
        printf("%-8s %-6s %-6s %s\n", pid, fd, get_type_str(st.st_mode), path);
    }
}

static void print_fd_entry(const char *pid, const char *fd_name, const char *fd_path) {
    char link_target[4096];
    ssize_t len = readlink(fd_path, link_target, sizeof(link_target) - 1);
    if (len != -1) {
        link_target[len] = '\0';
        print_entry(pid, fd_name, link_target);
    }
}

static int compare_ints(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

int execute_spy(char **args, int arg_count) {
    if (arg_count > 2) {
        printf("spy: invalid syntax\n");
        return 0;
    }
    char pid_str[32];
    if (arg_count == 1) {
        sprintf(pid_str, "%d", getpid());
    } else {
        strncpy(pid_str, args[1], 31);
        pid_str[31] = '\0';
    }
    
    char proc_path[256];
    sprintf(proc_path, "/proc/%s", pid_str);
    struct stat st;
    if (stat(proc_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("spy: no such process\n");
        return 0;
    }
    
    printf("%-8s %-6s %-6s %s\n", "PID", "FD", "TYPE", "PATH");
    
    char cwd_path[256];
    sprintf(cwd_path, "/proc/%s/cwd", pid_str);
    print_fd_entry(pid_str, "cwd", cwd_path);
    
    char exe_path[256];
    sprintf(exe_path, "/proc/%s/exe", pid_str);
    print_fd_entry(pid_str, "txt", exe_path);
    
    char maps_path[256];
    sprintf(maps_path, "/proc/%s/maps", pid_str);
    FILE *maps = fopen(maps_path, "r");
    if (maps) {
        char line[1024];
        char *seen[1024];
        int seen_count = 0;
        while (fgets(line, sizeof(line), maps)) {
            char *last_space = strrchr(line, ' ');
            if (last_space) {
                last_space++;
                char *newline = strchr(last_space, '\n');
                if (newline) *newline = '\0';
                
                if (last_space[0] == '/') {
                    int already_seen = 0;
                    for (int i = 0; i < seen_count; i++) {
                        if (strcmp(seen[i], last_space) == 0) {
                            already_seen = 1;
                            break;
                        }
                    }
                    if (!already_seen && seen_count < 1024) {
                        seen[seen_count++] = strdup(last_space);
                        print_entry(pid_str, "mem", last_space);
                    }
                }
            }
        }
        for (int i = 0; i < seen_count; i++) free(seen[i]);
        fclose(maps);
    }
    
    char fd_dir_path[256];
    sprintf(fd_dir_path, "/proc/%s/fd", pid_str);
    DIR *d = opendir(fd_dir_path);
    if (d) {
        struct dirent *dir;
        int fds[1024];
        int fd_count = 0;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] >= '0' && dir->d_name[0] <= '9') {
                if (fd_count < 1024) {
                    fds[fd_count++] = atoi(dir->d_name);
                }
            }
        }
        closedir(d);
        
        qsort(fds, fd_count, sizeof(int), compare_ints);
        
        for (int i = 0; i < fd_count; i++) {
            char fd_name[32];
            sprintf(fd_name, "%d", fds[i]);
            char fd_full_path[512];
            sprintf(fd_full_path, "%s/%s", fd_dir_path, fd_name);
            print_fd_entry(pid_str, fd_name, fd_full_path);
        }
    }
    
    return 0;
}
