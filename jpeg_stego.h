#ifndef JPEG_STEGO
#define JPEG_STEGO

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

#define JPEG_DQT 0xDB
#define JPEG_DHT 0xC4
#define JPEG_SOS 0xDA
#define JPEG_START 0xD8
#define JPEG_END 0xD9
#define JPEG_APPn_MIN 0xE0
#define JPEG_APPn_MAX 0xEF
#define JPEG_COM 0xFE
#define JPEG_COUNT_CHANEL 0x03
#define AC_OFFSET 2

struct DHT_tree {
	struct DHT_tree *left;
	struct DHT_tree *right;
	struct DHT_tree *prev;
	uint8_t value;
};

struct Tree_Par {
	uint16_t val_length;
	uint8_t tab_class;
	uint8_t tab_num;
};

struct Chanel_Par {
	uint8_t DC;
	uint8_t AC;
};

struct Jpg_Par {
	struct DHT_tree DC_AC[4];
	struct Chanel_Par Y;
	struct Chanel_Par CbCr;
	uint8_t check_tab;
	uint8_t DC_time;
	uint8_t check_c;
	uint8_t val;
};

struct Tmp_Trees {
	struct DHT_tree *cur_DC;
	struct DHT_tree *cur_AC;
	struct DHT_tree *temp;
};

void jpeg_start_stego(struct Img_Des *, struct Start_Par *);
#endif