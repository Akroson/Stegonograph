#ifndef PNG_STEGO
#define PNG_STEGO

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

#define PNG_IHDR 0x484452
#define PNG_IDAT 0x444154
#define PNG_IDAT_FULL 0x49444154
#define PNG_tEXt 0x74455874

#define READ_4_BYTE(num1, num2, num3, num4) ((Img->body[Img->ptr + (num1)] << 24) | (Img->body[Img->ptr + (num2)] << 16) | \
 	(Img->body[Img->ptr + (num3)] << 8) | Img->body[Img->ptr + (num4)])

struct Tree
{
	struct Tree *left;
	struct Tree *right;
	struct Tree *prev;
	uint16_t val;
};

struct huffm_elem
{
	uint16_t val;
	uint8_t len;
};

struct Png_Par
{
	struct Tree *tree_val;
	struct Tree *tree_shift;
	uint32_t IDAT_len;
	uint32_t sub_ptr;
	uint16_t CINFO;
	uint16_t line_val;
	uint16_t sub_len;
	uint8_t filter;
	uint8_t type;
};

void png_start_stego(struct Img_Des *, struct Start_Par *);
#endif