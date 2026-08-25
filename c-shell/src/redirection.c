#include "redirection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define COPY_BUFFER_SIZE 8192

int setup_input_redirection(char **args, int arg_count)
{
    int found_input = 0;
    FILE *tmp = NULL;
    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], "<") == 0)
        {
            found_input = 1;
            if (i + 1 >= arg_count)
            {
                fprintf(stderr, "cshell: no such file or directory\n");
                return -1;
            }
            if (tmp == NULL)
            {
                tmp = tmpfile();
                if (tmp == NULL)
                {
                    fprintf(stderr, "cshell: no such file or directory\n");
                    return -1;
                }
            }
            const char *filename = args[i + 1];
            int fd = open(filename, O_RDONLY);
            if (fd < 0)
            {
                fprintf(stderr, "cshell: no such file or directory\n");
                fclose(tmp);
                return -1;
            }
            char buffer[COPY_BUFFER_SIZE];
            ssize_t bytes_read;

            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
            {
                size_t written = 0;

                while (written < (size_t)bytes_read)
                {
                    size_t n = fwrite(buffer + written, 1, bytes_read - written, tmp);
                    if (n == 0)
                    {
                        close(fd);
                        fclose(tmp);
                        return -1;
                    }
                    written += n;
                }
            }
            if (bytes_read < 0)
            {
                close(fd);
                fclose(tmp);
                return -1;
            }
            close(fd);
            i++;
        }
    }
    if (!found_input)
    {
        return 0;
    }
    fflush(tmp);
    if (lseek(fileno(tmp), 0, SEEK_SET) == (off_t)-1)
    {
        fclose(tmp);
        return -1;
    }
    if (dup2(fileno(tmp), STDIN_FILENO) < 0)
    {
        fclose(tmp);
        return -1;
    }
    fclose(tmp);
    return 1;
}

int setup_output_redirection(char **args, int arg_count, FILE **out_tmp)
{
    int found_output = 0;
    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0)
        {
            found_output = 1;
            if (i + 1 >= arg_count)
            {
                fprintf(stderr,
                        "cshell: unable to create file for writing\n");
                return -1;
            }
            const char *filename = args[i + 1];
            int flags;
            if (strcmp(args[i], ">") == 0)
            {
                flags = O_WRONLY | O_CREAT | O_TRUNC;
            }
            else
            {
                flags = O_WRONLY | O_CREAT | O_APPEND;
            }
            int fd = open(filename, flags, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "cshell: unable to create file for writing\n");
                return -1;
            }
            close(fd);
            i++;
        }
    }
    if (!found_output)
    {
        *out_tmp = NULL;
        return 0;
    }
    FILE *tmp = tmpfile();
    if (tmp == NULL)
    {
        fprintf(stderr,
                "cshell: unable to create file for writing\n");
        return -1;
    }
    if (dup2(fileno(tmp), STDOUT_FILENO) < 0)
    {
        fclose(tmp);
        return -1;
    }
    *out_tmp = tmp;
    return 1;
}
int distribute_output(char **args, int arg_count, FILE *out_tmp)
{
    if (out_tmp == NULL)
    {
        return 0;
    }
    fflush(out_tmp);
    if (lseek(fileno(out_tmp), 0, SEEK_SET) == (off_t)-1)
    {
        fclose(out_tmp);
        return -1;
    }
    size_t capacity = 8192;
    size_t size = 0;
    char *data = malloc(capacity);
    if (data == NULL)
    {
        fclose(out_tmp);
        return -1;
    }
    while (1)
    {
        if (size == capacity)
        {
            capacity *= 2;
            char *new_data = realloc(data, capacity);
            if (new_data == NULL)
            {
                free(data);
                fclose(out_tmp);
                return -1;
            }
            data = new_data;
        }
        ssize_t n = read(fileno(out_tmp),
                         data + size,
                         capacity - size);
        if (n < 0)
        {
            free(data);
            fclose(out_tmp);
            return -1;
        }
        if (n == 0)
        {
            break;
        }
        size += n;
    }
    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0)
        {
            if (i + 1 >= arg_count)
            {
                free(data);
                fclose(out_tmp);
                return -1;
            }
            const char *filename = args[i + 1];
            int flags;
            if (strcmp(args[i], ">") == 0)
            {
                flags = O_WRONLY | O_CREAT | O_TRUNC;
            }
            else
            {
                flags = O_WRONLY | O_CREAT | O_APPEND;
            }
            int fd = open(filename, flags, 0644);
            if (fd < 0)
            {
                free(data);
                fclose(out_tmp);
                fprintf(stderr, "cshell: unable to create file for writing\n");
                return -1;
            }
            size_t written = 0;
            while (written < size)
            {
                ssize_t n = write(fd, data + written, size - written);
                if (n < 0)
                {
                    close(fd);
                    free(data);
                    fclose(out_tmp);
                    return -1;
                }
                written += n;
            }
            close(fd);
            i++;
        }
    }
    free(data);
    fclose(out_tmp);
    return 0;
}