#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#define BUF_SIZE 128

/*
 * Функция проверяет имя файла @filename на соответствие шаблону @pattern.
 * Шаблон имеет следующий вид: xxx, xxx*xxx, *xxx, xxx*, где '*' совпадает с
 * любой последовательностью символов.
 * Возвращает true, если имя совпадает с шаблоном, иначе false.
 */
static bool match_pattern(const char *filename, const char *pattern)
{
    char *patternPointer = strchr(pattern, '*');
    if (!patternPointer) // "xxx"
    {
        return *filename == *pattern;
    }
    else if (patternPointer == pattern) // "*-----"
    {

        char *patternPointerToRight = patternPointer;
        ++patternPointerToRight;

        // "*"
        if (*patternPointerToRight == '\0')
            return true;

        char newPattern[strlen(pattern)];

        memmove(newPattern, pattern + 1, strlen(pattern));
        size_t i = strlen(filename);
        size_t j = strlen(newPattern);
        while (j > 0)
        {
            if (filename[i--] != newPattern[j--])
            {
                return false;
            }
        }
        return true;
    } else 
    {
        char *patternPointerToRight = patternPointer;
        ++patternPointerToRight;

        // "------*"
        if (*patternPointerToRight == '\0')
        {
            char newPattern[strlen(pattern)];
            
            memmove(newPattern, pattern, strlen(pattern)-1);
            newPattern[strlen(pattern) - 1] = '\0';
            
            for (size_t i = 0; i < strlen(newPattern); i++)
            {
                if (filename[i] != newPattern[i])
                {
                    return false;
                }
            }
            return true;
        } else
        {
            for (size_t i = 0; pattern[i] != '*'; i++)
            {
                if (filename[i] != pattern[i])
                {
                    return false;
                }
            }
            size_t i = strlen(filename);
            size_t j = strlen(pattern);
            for (; pattern[j] != '*'; --i, --j)
            {
                if (filename[i] != pattern[j])
                {
                    return false;
                }
            }
            
        }
        
    }
    

    return true;
}

/*
 * Функция перебирает все файлы в текущем каталоге и выводит в терминал те
 * имена файлов, которые совпадают с шаблоном @pattern. Совпадение проверяется
 * с помощью функции match_pattern(). Чтение каталога может производится
 * стандартными функциями opendir(), readdir, closedir().
 */
static int run_ls(const char *pattern)
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./");
    if (dir != NULL)
    {
        while (entry = readdir(dir))
        {
            if (match_pattern(entry->d_name, pattern))
            {
                puts(entry->d_name);
            }
        }
        closedir(dir);
    } else
    {
        puts("Couldn't open the directory!");
    }
    return 0;
}

/*
 * Функция обрабатывает специальные последовательности "\n" (переход на новую
 * строку) и "\b" (удаление предшествующего символа) в строке @echo_data и
 * записывает результат в файл @file_name. Обработка @echo_data может
 * производиться с помощью strchr() и memmove(), запись в файл - с помощью
 * fopen() и fwrite().
 */
static int run_echo(const char *file_name, const char *echo_data)
{
    FILE *fp;

    fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        printf("Can't open %s for writing", file_name);
    }
    else
    {
        fputs(echo_data, fp);
        fprintf(fp, "\n");
        fclose(fp);
    }
    return 0;
}

/*
 * Функция выводит в терминал содержимое файла @file_name с помощью стандартных
 * функций fopen(), fread(), fclose().
 */
static int run_cat(const char *file_name)
{

    FILE *fp;

    fp = fopen(file_name, "r");
    if (fp == NULL)
    {
        printf("Can't open %s for reading", file_name);
    }
    else
    {
        char stringFromFile[BUF_SIZE];
        while (!feof(fp))
        {
            fgets(stringFromFile, BUF_SIZE, fp);
            fprintf(stdout, "%s", stringFromFile);
            stringFromFile[0] = '\0';
        }
        fclose(fp);
    }

    return 0;
}

static int run_touch(const char *file_name)
{
    FILE *fp;

    printf("Executing 'touch' of file %s\n", file_name);
    fp = fopen(file_name, "a");
    if (!fp)
    {
        fprintf(stderr, "Failed to touch file '%s': %s\n",
                file_name, strerror(errno));
        return 0;
    }
    fclose(fp);
    return 0;
}

static int parse_command(const char *command)
{
    char cmd_data[BUF_SIZE], echo_data[BUF_SIZE], file_name[BUF_SIZE];
    int ret;

    ret = sscanf(command, "ls %s", cmd_data);
    if (ret == 1)
        return run_ls(cmd_data);

    ret = sscanf(command, "touch %s", file_name);
    if (ret == 1)
        return run_touch(file_name);

    ret = sscanf(command, "cat %s", file_name);
    if (ret == 1)
        return run_cat(file_name);

    ret = sscanf(command, "echo %s", cmd_data);
    if (ret == 1)
    {
        char *srcPointer = strchr(command, ' ');
        while (*(++srcPointer) == ' ')
            ;
        size_t i = 0;
        for (; *srcPointer != '>'; i++, srcPointer++)
        {
            if (*srcPointer == '\\')
            {
                ++srcPointer;
                if (*srcPointer == 'n')
                {
                    echo_data[i] = '\n';
                }
                else if (*srcPointer == 'b')
                {
                    i -= 2;
                }
                else
                {
                    --srcPointer;
                }
            }
            else
            {
                echo_data[i] = *srcPointer;
            }
        }
        echo_data[i] = '\0';
        while (*(++srcPointer) == ' ')
            ;

        for (i = 0; *srcPointer != '\n' && *srcPointer != ' '; i++, srcPointer++)
        {
            file_name[i] = *srcPointer;
        }
        file_name[i] = '\0';
        return run_echo(file_name, echo_data);
    }

    fprintf(stderr, "Unknown command '%s'\n", command);
    return 0;
}

int main(int argc, char *argv[])
{
    char command[BUF_SIZE];
    int ret;

    
    

    while (1)
    {
        ret = read(STDIN_FILENO, command, sizeof(command) - 1);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to read command from stdin: %s\n",
                    strerror(errno));
            goto on_error;
        }

        command[ret] = '\0';
        ret = parse_command(command);
        if (ret < 0)
            goto on_error;
    }

    return 0;
on_error:
    return 1;
}
