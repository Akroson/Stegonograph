#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "helper.h"

uint8_t check_exp(FILE *ptr)
{
	uint8_t i, j, byte, size;
	uint8_t jpeg_exp[] = {0xFF, 0xD8, 0x00};
	uint8_t png_exp[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00};
	uint8_t *ptr_arr[] = {
		jpeg_exp,
		png_exp,
	};

	size = sizeof(ptr_arr)/sizeof(uint8_t *);
	byte = getc(ptr);

	for (i = 0; i < size; i++) {
		if (byte == ptr_arr[i][0]) {
			j = 1;
			while (1) {
				byte = getc(ptr);

				if (ptr_arr[i][j] == 0x00)
					break;

				if (ptr_arr[i][j] == byte)
					j++;
				else return 0;
			}
			break;
		}
	}

	fseek(ptr, 0, SEEK_SET);
	return ++i;
}

void error_alert(char *str) {
	fputs(str, stderr);
	exit(1);
}

void get_user_stirng(struct Line *line)
{
	char *buff;
	char word;
	uint8_t i, size_buf;

	i = 0;
	size_buf = 10;
	buff = NULL;
	buff = (char *)malloc(sizeof(char) * size_buf);
	if (buff == NULL)
		error_alert("Memory allocation error");

	printf("%s ", "Text:");

	while ((word = fgetc(stdin)) != '\n' && word != EOF) {
		if ((i + 1) > size_buf) {
			if (size_buf >= 160) {
				break;
			} else {
				size_buf *= 2;
			}

			buff = realloc(buff, sizeof(char) * size_buf);
			if (buff == NULL){
				free(buff);
				line->str = NULL;
				return;
			}
		}

		buff[i++] = word;
	}
	printf("\n");

	if (i == 0 && word == EOF) {
		free(buff);
		line->str = NULL;
		return;
	}
	line->str = (char *)malloc(sizeof(char) * (i + 1));
	strncpy(line->str, buff, i);
	line->str_len = i + 1;
	line->str[i] = '\0';
    free(buff);
}

char* get_stego_name(struct Start_Par *Par)
{
	char *stego_name;
	char exp_file[][4]= {"jpg", "png"};
	uint8_t pos;
	pos = strlen(Par->file_name);
	while (pos--) {
		if (Par->file_name[pos] == '\\' || Par->file_name[pos] == '/') break;
	}

	stego_name = (char *)calloc((pos + 10), sizeof(char));
	strncpy(stego_name, Par->file_name, pos + 1);
	strcat(stego_name, "stego.");
	strcat(stego_name, exp_file[Par->type - 1]);
	stego_name[pos + 10] = '\0';
	free(Par->file_name);

	return stego_name;
}

void parse_comnd_line(int argc, char *argv[], struct Start_Par *Par)
{
	if (argc == 2 && !strcmp(argv[1], "-help")) {
		error_alert("Order token:<name prog> <full path img> <token (\"-sr\" or \"-sw\")>");
	} else if (argc < 3) {
		error_alert("Not enough parameters");
	}

	for (uint8_t i = 1; i < argc; i++) {
		if (strlen(argv[i]) <= 4 && i > 1) {
			if (!strcmp(argv[i], "-sw")) {
				Par->action = 1;
			} else if (!strcmp(argv[i], "-sr")) {
				Par->action = 2;
			}
		} else if (strlen(argv[i]) > 4 && i == 1) {
			Par->file_name = argv[i];
		} else {
			error_alert("Incorrect input");
		}
	}
}

void stego_write(struct Img_Des *Img, struct Line *line) 
{
	uint8_t line_bit, img_bit;
	Img->bit--;
	line_bit = line->str[line->num] & (0x80 >> line->bit_set);
	img_bit = Img->body[Img->ptr] & (0x01 << Img->bit);

	if (line_bit && !img_bit || !line_bit && img_bit) {
		Img->body[Img->ptr] ^= 0x01 << Img->bit;
	}

	Img->bit++;
	line->bit_set++;
}

void stego_read(struct Img_Des *Img, struct Line *line)
{	
	if (Img->body[Img->ptr] & (0x01 << (Img->bit - 1))) {
		line->str[line->num] |= (0x80 >> line->bit_set);
	} else {
		line->str[line->num] &= ~(0x80 >> line->bit_set);
	}

	line->bit_set++;
}

void img_add_read(struct Img_Des *Img, struct Line *line)
{
	if (!line->str_len || !line->str[line->num]) {
		if (!line->str_len) line->str[line->num] = '\0';
		printf("Text: %s\n", line->str);
		exit(1);
	} else if ((line->num + 1) == line->str_len) {
		line->str_len += 10;
		line->str = (char *)realloc(line->str, sizeof(char) * line->str_len);
	}
}

void img_add_write(struct Img_Des *Img, struct Line *line)
{
	if ((line->num + 1) == line->str_len || !line->str[line->num]) {
		FILE *img = fopen(Img->stego_name, "wb");
		fwrite(Img->body, 1, Img->size, img);
		fclose(img);
		exit(1);
	}
}