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
#include <locale.h>

#define BUF_SIZE 128

/*
 * Функция проверяет имя файла @filename на соответствие шаблону @pattern.
 * Шаблон имеет следующий вид: xxx, xxx*xxx, *xxx, xxx*, где '*' совпадает с
 * любой последовательностью символов.
 * Возвращает true, если имя совпадает с шаблоном, иначе false.
 */
static bool match_pattern(const char *filename, const char *pattern)
{

	/* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */

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

	/* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */

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
    FILE* fp;
    fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        printf("Couldn't open %s for writing\n", file_name);
    }
    else
    {
        fprintf(fp, "%s", echo_data);
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
    FILE* fp;
    fp = fopen(file_name, "r");
    if (fp == NULL)
    {
        printf("Couldn't open %s for reading\n", file_name);
    }
    else
    {
        char strFromFile[BUF_SIZE];
        while (!feof(fp))
        {
	    fgets(strFromFile,BUF_SIZE,fp);
            printf("%s", strFromFile);
        }
	printf("\n");
    fclose(fp);
    }
    return 0;
}

static int run_touch(const char *file_name)
{
	FILE *fp;

	printf("Executing 'touch' of file %s\n", file_name);
	fp = fopen(file_name, "a");
	if (!fp) {
		fprintf(stderr, "Failed to touch file '%s': %s\n",
				file_name, strerror(errno));
		return 0;
	}
	fclose(fp);
	return 0;
}

static int parse_command(const char *command)
{
	char cmd_data[BUF_SIZE], file_name[BUF_SIZE];
	int ret;
	char echo_data[BUF_SIZE];

	ret = sscanf(command, "touch %s", file_name);
	if (ret == 1)
		return run_touch(file_name);

	ret = sscanf(command, "cat %s", file_name);
	if (ret == 1)
		return run_cat(file_name);

	ret = sscanf(command, "echo %s", cmd_data);
	if (ret == 1)
	{
		char *srcPtr;
		srcPtr = strchr(command, ' ');
		size_t i = 0;

		while (*(++srcPtr) == ' ')
			;
		for (; *srcPtr != '>'; i++)
		{
			echo_data[i] = *(srcPtr++);
		}
		echo_data[i] = '\n';
		while (*(++srcPtr) == ' ')
			;
		i = 0;
		for (; *srcPtr != '\n'; ++srcPtr)
		{
			file_name[i++] = *srcPtr;
		}
		file_name[i] = '\0';
		return run_echo(file_name, echo_data);
	}

	fprintf(stderr, "Unknown command '%s'\n", command);
	return 0;
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "Rus");
	char command[BUF_SIZE];

	while (1) {
		fgets(command, BUF_SIZE, stdin);
		int ret = parse_command(command);
		if (ret < 0) {
			fprintf(stderr, "Failed to read command from stdin: %s\n",
					strerror(errno));
			goto on_error;
		}
	}

	return 0;
on_error:
	return 1;
}
