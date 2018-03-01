#include "jpeg_stego.h"

static void(*jpg_do_something)(struct Img_Des *, struct Line *);
static void(*jpg_do_something_else)(struct Img_Des *, struct Line *);

static void fill_arr_DHT(struct Img_Des *Img, uint8_t *HT_arr, uint8_t *val_arr, uint16_t val_length)
{
	Img->ptr += 5;

	for (uint8_t i = 0; i < 16; i++) {
		HT_arr[i] = Img->body[Img->ptr++];
	}
	
	for (uint16_t j = 0; j <= val_length; j++) {
		val_arr[j] = Img->body[Img->ptr++];
	}

	Img->ptr -= 2;
}

static void jpeg_create_DHT(struct Jpg_Par *Param, struct Img_Des *Img, struct Tree_Par *TP)
{
	uint16_t counter = 0;
	uint8_t HT_arr[16];
	uint8_t val_arr[TP->val_length];
	uint8_t hight, cur_hight, val_hight;

	TP->tab_num += TP->tab_class == 0 ? 0 : AC_OFFSET;
	struct DHT_tree *tree_DHT = &Param->DC_AC[TP->tab_num];

	cur_hight = 1;
	tree_DHT->left = tree_DHT->right = tree_DHT->prev = NULL;

	fill_arr_DHT(Img, HT_arr, val_arr, TP->val_length);

	for (uint8_t i = 0; i < 16; i++) {
		hight = i + 1;
		val_hight = HT_arr[i];
		while (val_hight) {
			if (tree_DHT->left == NULL)  {
				tree_DHT->left = (struct DHT_tree *)malloc(sizeof(struct DHT_tree));
				tree_DHT->left->left = tree_DHT->left->right = NULL;
				if (cur_hight == hight) {
					tree_DHT->left->value = val_arr[counter++];
					val_hight--;
				} else {
					tree_DHT->left->prev = tree_DHT;
					tree_DHT = tree_DHT->left;
					cur_hight++;
				}
			} else if (tree_DHT->right == NULL) {
				tree_DHT->right = (struct DHT_tree *)malloc(sizeof(struct DHT_tree));
				tree_DHT->right->left = tree_DHT->right->right = NULL;
				if (cur_hight == hight) {
					tree_DHT->right->value = val_arr[counter++];
					val_hight--;
				} else {
					tree_DHT->right->prev = tree_DHT;
					tree_DHT = tree_DHT->right;
					cur_hight++;
				}
			} else if (tree_DHT->prev != NULL) {
				tree_DHT = tree_DHT->prev;
				cur_hight--;
			}
		}
	}
}

static void jpeg_parse_head(struct Img_Des *Img, struct Jpg_Par *Param)
{
	struct Tree_Par TP;
	uint8_t next_byte, len, check_finish;
	check_finish = 0;

	while (1) {
		if (Img->body[Img->ptr] == 0xFF) {
			next_byte = Img->body[Img->ptr + 1];
			if (next_byte == JPEG_START) {
				check_finish++;
			} else if (next_byte == JPEG_COM || (next_byte >= JPEG_APPn_MIN && next_byte <= JPEG_APPn_MAX)) {
				Img->ptr += (((Img->body[Img->ptr + 2]) << 8) | Img->body[Img->ptr + 3]) + 2;
			} else if (next_byte == JPEG_DHT && (check_finish == 1)) {

				TP.val_length = (((Img->body[Img->ptr + 2]) << 8) | Img->body[Img->ptr + 3]) - 19;
				TP.tab_class = Img->body[Img->ptr + 4] & 0x10;
				TP.tab_num = Img->body[Img->ptr + 4] & 0x0F;

				jpeg_create_DHT(Param, Img, &TP);
			} else if (next_byte == JPEG_SOS) {
				len = ((Img->body[Img->ptr + 2] << 8) | Img->body[Img->ptr + 3]) + 2;

				if (Img->body[Img->ptr + 4] != JPEG_COUNT_CHANEL) 
					error_alert("Unsupported jpeg");

				if (Img->body[Img->ptr + 5] == 0x01) {
					Param->Y.DC = (Img->body[Img->ptr + 6] & 0xF0) >> 4;
					Param->Y.AC = Img->body[Img->ptr + 6] & 0x0F;
				}

				if (Img->body[Img->ptr + 7] == 0x02 && Img->body[Img->ptr + 9] == 0x03) {
					if (Img->body[Img->ptr + 8] == Img->body[Img->ptr + 10]) {
						Param->CbCr.DC = (Img->body[Img->ptr + 10] & 0xF0) >> 4;
						Param->CbCr.AC = Img->body[Img->ptr + 10] & 0x0F;
					} else error_alert("Unsupported jpeg");
				} else error_alert("Unsupported jpeg");

				Img->ptr += len;
				break;
			} else if (next_byte == JPEG_END) {
				check_finish--;
			}
		}
		Img->ptr++;
	}
}

static uint8_t check_find_val(struct Jpg_Par *Param, struct Img_Des *Img, struct Tmp_Trees *Trees)
{
	Param->val = Trees->temp->value;
	if (Param->DC_time) {
		Trees->temp = Trees->cur_AC;
		Param->DC_time--;

		if (Param->val != 0) {
			Param->val &= 0x0F;
			return 1;
		}
	} else if (Param->val == 0) {
		Param->check_c++;
		Param->DC_time++;
		Param->check_tab++;
		if (Param->check_tab == 4) {
			Trees->cur_DC = &Param->DC_AC[Param->CbCr.DC];
			Trees->cur_AC = &Param->DC_AC[Param->CbCr.AC + AC_OFFSET];
		} else if (Param->check_tab == 6) {
			if ((Img->ptr + 1) == Img->size) return 0;

			Param->check_tab = 0;
			Trees->cur_DC = &Param->DC_AC[Param->Y.DC];
			Trees->cur_AC = &Param->DC_AC[Param->Y.AC + AC_OFFSET];
		}
		Trees->temp = Trees->cur_DC;

	} else {
		Trees->temp = Trees->cur_AC;
		Param->val &= 0x0F;
		return 1;
	}

	return 2;
}

//TODO: incorect read, breaks the image
static void jpeg_stego_loop(struct Img_Des *Img, struct Jpg_Par *Param, struct Line *line)
{
	struct Tmp_Trees Trees;
	uint8_t tmp;

	Param->check_tab = Img->bit = 0;
	Param->DC_time = Param->check_c = 1;
	Trees.cur_DC = &Param->DC_AC[Param->Y.DC];
	Trees.cur_AC = &Param->DC_AC[Param->Y.AC + AC_OFFSET];
	Trees.temp = Trees.cur_DC;

	while (1) {
		if (Img->bit > 7) {
			Img->bit = 0;
			Img->ptr++;
		}

		if (Img->body[Img->ptr] & (0x80 >> Img->bit)) {
			Trees.temp = Trees.temp->right;
		} else {
			Trees.temp = Trees.temp->left;
		}

		if (!Trees.temp->left && !Trees.temp->right) {
			tmp = check_find_val(Param, Img, &Trees);
			if (!tmp) {
				if (line->str_len > 160) {
					line->str_len = 0;
					fputs("Process was not complete\n", stderr);
					exit(1);
				}
				
				jpg_do_something_else(Img, line);
			} else if (tmp == 1) {
				Img->bit++;

				if (Img->bit == 8) {
					Img->ptr++;
					Img->bit = Param->val;
				} else {
					Img->bit += Param->val;
				}

				//if first bit equal ziro - it's negative value
				tmp = Param->val > 1 ? Img->body[Img->ptr] & (0x80 >> (Img->bit - Param->val)) : 0;

				if (Img->bit > 8 || Img->bit >= 15) {
					Img->bit -= 8;
					Img->ptr++;

					if (Img->bit >= 8) {
						Img->bit -= 8;
						Img->ptr++;
					}

					if (Img->bit == 0) Img->bit = 1;
				} 

				if (Param->check_tab < 4 && tmp && !Param->DC_time) {
					jpg_do_something(Img, line);
					Param->check_c--;

					if (line->bit_set == 8) {
						line->bit_set = 0;
						jpg_do_something_else(Img, line);
						line->num++;
					}
				}

				if (Param->DC_time) Param->DC_time--;
			}
		}

		Img->bit++;
	}
}

void jpeg_start_stego(struct Img_Des *Img, struct Start_Par *Str_Par)
{
	struct Jpg_Par Param;
	struct Line line;
	Img->size -= 2;
	Img->ptr = line.num = line.bit_set = 0;
	line.str = NULL;
	jpeg_parse_head(Img, &Param);

	if (Str_Par->action == 1) {
		get_user_stirng(&line);
		if (line.str == NULL)
			error_alert("Incorrect input");
		Img->stego_name = get_stego_name(Str_Par);
		line.status = 1;
		jpg_do_something = stego_write;
		jpg_do_something_else = img_add_write;
	} else if (Str_Par->action == 2) {
		line.status = 2;
		line.str_len = 10;
		line.str = (char *)malloc(10 * sizeof(char));
		jpg_do_something = stego_read;
		jpg_do_something_else = img_add_read;
	}

	jpeg_stego_loop(Img, &Param, &line);
}
