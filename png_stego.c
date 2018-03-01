#include "png_stego.h"

static uint8_t table_shift[] = 
	{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 
	5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
	11, 11, 12, 12, 13, 13};

static uint8_t table_len[][2] = 
{
	{0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {0, 8}, {0, 9}, {0, 10},
	{1, 11}, {1, 13}, {1, 15}, {1, 17}, {2, 19}, {2, 23}, {2, 27},
	{2, 31}, {3, 35}, {3, 43}, {3, 51}, {3, 59}, {4, 67}, {4, 83},
	{4, 99}, {4, 115}, {5, 131}, {5, 163}, {5, 195}, {5, 227}, {0, 255}
};

static void(*png_do_something)(struct Img_Des *, struct Line *);
static void(*png_do_something_else)(struct Img_Des *, struct Line *);
static void(*png_do_something_with_this)(struct Line *, struct Line *, uint8_t);

static uint8_t convert_5bit(uint8_t val)
{
	uint8_t tmp = 0;

	tmp |= (val & 0x10) >> 4;
	tmp |= (val & 0x08) >> 2;
	tmp |= (val & 0x04);
	tmp |= (val & 0x02) << 2;
	tmp |= (val & 0x01) << 4;

	return tmp;
}

static uint16_t convert_7bit(uint16_t val)
{
	uint16_t tmp = 0;

	tmp |= (val & 0x40) >> 6;
	tmp |= (val & 0x20) >> 4;
	tmp |= (val & 0x10) >> 2;
	tmp |= (val & 0x08);
	tmp |= (val & 0x04) << 2;
	tmp |= (val & 0x02) << 4;
	tmp |= (val & 0x01) << 6;

	return tmp;
}

static uint8_t comp_pix_val(uint8_t depth, uint8_t color)
{
	switch (color) {
		case 0:
			return (1 * depth);
		case 2:
			return (3 * depth);
		case 4:
			return (2 * depth);
		case 6:
			return (4 * depth);
		default:
			error_alert("Unsupported png");
	}
}

static uint8_t val_table_check(uint16_t val, uint8_t *type, uint8_t *tmp)
{
	uint8_t check = val & 0x07;

	if ((check != 3 && check <= 6 && check >= 1) && (val >= 3 && val <= 125)) {
		*type = 1;
		*tmp = 1;
	} else if (!(val & 0x03) && (val > 0 && val <= 116)) {
		*type = 3;
		*tmp = 0;
	} else if ((val & 0x1F) == 3 && (val >= 3 && val <= 223)) {
		*type = 4;
		*tmp = 1;
	} else if ((val & 0x03) == 3 && ((val >= 7 && val <= 127))) {
		*type = 2;
		*tmp = 2;
	} else if (!val) {
		return 0;
	}

	return 1;
}

static uint16_t read_bit(struct Img_Des *Img, uint8_t count)
{
	uint16_t val;

	if (Img->bit >= 8) {
		Img->bit -= 8;
		Img->ptr++;
	}

	if (Img->bit >= (9 - count)) {
		val = (Img->body[Img->ptr++] & (Img->help_read[count] << Img->bit)) >> Img->bit;
		Img->bit = (Img->bit + count) - 8;
		val |= (Img->body[Img->ptr] & Img->help_read[Img->bit]) << (count - Img->bit);
	} else {
		val = (Img->body[Img->ptr] & (Img->help_read[count] << Img->bit)) >> Img->bit;
		Img->bit += count;
	}

	return val;
}

static uint32_t adler32(char *arr, uint8_t len)
{
	uint32_t sum1 = 1;
	uint32_t sum2 = 0;

	if (len == 1) {
		sum1 += arr[0];
		sum2 += sum1;

		return sum1 | (sum2 << 16);
	}

	while (len > 1) {
		len -= 2;
		sum1 += (uint32_t)arr[0];
		sum2 += sum1;
		sum1 += (uint32_t)arr[1];
		sum2 += sum1;
		arr += 2;
	}

	if (len) {
		sum1 += arr[0];
		sum2 += sum1;
	}

	sum1 %= 65521;
	sum2 %= 65521;

	return sum1 | (sum2 << 16);
}

static void free_tree(struct Tree *tree)
{
	if (!tree->left->left && !tree->left->right) {
		free(tree->left);
	} else {
		free_tree(tree->left);
	}

	if (!tree->right->left && !tree->right->right) {
		free(tree->right);
	} else {
		free_tree(tree->right);
	}

	free(tree);
}

static void dun_stego_read(struct Line *line, struct Line *sub_line, uint8_t val)
{	
	if (line->str[line->num] & (0x80 >> line->bit_set++)) {
		if ((val & 1)) {
			sub_line->str[sub_line->num] |= (0x80 >> sub_line->bit_set);
		}
		sub_line->bit_set++;
	}
}

static void dun_stego_write(struct Line *line, struct Line *sub_line, uint8_t val)
{
	uint8_t tmp = line->str[line->num] & (0x80 >> line->bit_set);

	if (((val & 1) && tmp) || (!(val & 1) && !tmp)) {
		line->bit_set++;
		sub_line->str[sub_line->num] |= (0x80 >> sub_line->bit_set);
	}

	sub_line->bit_set++;
}

static void swap_huff_elem(struct huffm_elem *elem1, struct huffm_elem *elem2)
{
	struct huffm_elem tmp;
	tmp = *elem1;
	*elem1 = *elem2;
	*elem2 = tmp;
}


static void sort_huff_elem(struct huffm_elem *arr, int16_t size) {
	for (uint16_t i = 0; i < size; i++) {
		for (uint16_t j = i; j < size; j++) {
			if (arr[i].len > arr[j].len) {
				swap_huff_elem(&arr[i], &arr[j]);
			} else if ((arr[i].len == arr[j].len) && (arr[i].val > arr[j].val)) {
				swap_huff_elem(&arr[i], &arr[j]);
			}
		}
	}
}

static void fill_arr_huff_elem(struct huffm_elem *elem, uint8_t *arr, uint16_t size_elem)
{
	uint16_t num = 0;

	for (uint16_t i = 0; i < size_elem; i++) {
		if (arr[i]) {
			elem[num].val = i;
			elem[num].len = arr[i];
			num++;
		}
	}
}

static struct Tree* create_tree(uint8_t *arr, uint16_t count, uint16_t size_elem)
{
	struct Tree *tree, *begin;
	struct huffm_elem elem[count];
	uint8_t cur_hight = 1;

	fill_arr_huff_elem(elem, arr, size_elem);
	sort_huff_elem(elem, count);

	tree = (struct Tree *)malloc(sizeof(struct Tree));
	tree->left = tree->right = tree->prev = NULL;
	begin = tree;

	for (uint16_t i = 0; i < count; i++) {
		while (1) {
			if (tree->left == NULL)  {
				tree->left = (struct Tree *)malloc(sizeof(struct Tree));
				tree->left->left = tree->left->right = NULL;
				if (cur_hight == elem[i].len) {
					tree->left->val = elem[i].val;
					break;
				} else {
					tree->left->prev = tree;
					tree = tree->left;
					cur_hight++;
				}
			} else if (tree->right == NULL) {
				tree->right = (struct Tree *)malloc(sizeof(struct Tree));
				tree->right->left = tree->right->right = NULL;
				if (cur_hight == elem[i].len) {
					tree->right->val = elem[i].val;
					break;
				} else {
					tree->right->prev = tree;
					tree = tree->right;
					cur_hight++;
				}
			} else if (tree->prev != NULL) {
				tree = tree->prev;
				cur_hight--;
			}
		}
	}

	return begin;
}

static struct Tree* create_alphabet(struct Img_Des *Img, uint8_t len)
{
	uint8_t alph[19] = {0};
	uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	uint8_t i, val, count;
	i = count = 0;

	for (; len; i++) {
		val = (uint8_t)read_bit(Img, 3);
		if (val) count++;
		alph[order[i]] = val;
		len--;
	}

	return create_tree(alph, (uint16_t)count, 19);
}

static struct Tree* create_tree_huff_val(struct Tree *Alph, struct Img_Des *Img, uint16_t len)
{
	struct Tree *tmp_tree = Alph;
	uint16_t tree_val, num, count;
	uint8_t arr_val[len];
	uint8_t val, shift;

	num = count = shift = 0;

	while (num <= len) {
		if (Img->bit >= 8) {
			Img->bit = 0;
			Img->ptr++;
		}

		if (Img->body[Img->ptr] & (0x01 << Img->bit)) {
			tmp_tree = tmp_tree->right;
		} else {
			tmp_tree = tmp_tree->left;
		}

		if (!tmp_tree->left && !tmp_tree->right) {
			tree_val = tmp_tree->val;
			tmp_tree = Alph;

			if (tree_val < 16) {
				arr_val[num++] = tree_val;
				if (tree_val) count++;
			} else {
				val = 0;
				Img->bit++;
				if (tree_val == 16) {
					val = arr_val[num - 1];
					shift = 3 + (uint8_t)read_bit(Img, 2);
					count += shift;
				} else if (tree_val == 17)  {
					shift = 3 + (uint8_t)read_bit(Img, 3);
				} else {
					shift = 11 + (uint8_t)read_bit(Img, 7);
				}
				Img->bit--;
			}

			while (shift) {
				arr_val[num++] = val;
				shift--;
			}
		}

		Img->bit++;
	}

	return create_tree(arr_val, count, num);
}

static void parse_dunam_huff(struct Img_Des *Img, struct Png_Par *Param)
{
	struct Tree *Alph;
	uint16_t HLIT, HDIST;
	uint8_t HCLEN;
	Img->bit = 1;
	HLIT = ((Img->body[Img->ptr++] & 0xF8) >> 3) + 256;
	HDIST = (Img->body[Img->ptr] & 0x1F);
	HCLEN = (((Img->body[Img->ptr++] & 0xE0) >> 5) | ((Img->body[Img->ptr] & 0x01) << 3)) + 4;
	Alph = create_alphabet(Img, HCLEN);

	Param->tree_val = create_tree_huff_val(Alph, Img, HLIT);

	if (HDIST >= 1) {
		Param->tree_shift = create_tree_huff_val(Alph, Img, HDIST);
	}

	free_tree(Alph);
}

static void check_field(struct Png_Par *Param, struct Img_Des *Img)
{	
	uint32_t tmp_IDAT, field;
	field = READ_4_BYTE(0, 1, 2, 3);

	if (field == PNG_tEXt) {
		Param->sub_ptr = Img->ptr - 1;
	} else if (field == PNG_IDAT_FULL) {
		tmp_IDAT = READ_4_BYTE(-4, -3, -2, -1);
		Img->ptr += tmp_IDAT + 6;
		check_field(Param, Img);
		Img->ptr -= tmp_IDAT + 6;
	} else {
		Param->sub_ptr = Img->ptr - 4;
		Param->sub_len = 1;
	}
}

static void png_parse_head(struct Img_Des *Img, struct Png_Par *Param)
{
	uint32_t field, length_w;
	uint8_t pix_val, type_enc;

	while (1) {
		if (Img->body[Img->ptr] == 0x49) {
			field = (Img->body[Img->ptr + 1] << 16) | (Img->body[Img->ptr + 2] << 8) | Img->body[Img->ptr + 3];
			if (field == PNG_IHDR) {
				length_w = READ_4_BYTE(4, 5, 6, 7);
				pix_val = comp_pix_val(Img->body[Img->ptr + 12], Img->body[Img->ptr + 13]);

				Param->line_val = (pix_val * length_w) / 8;
				Img->ptr += 16;
			} else if (field == PNG_IDAT) {
				if ((Img->body[Img->ptr + 4] & 0x0F) != 8)
					error_alert("Unsupported png");

				Param->IDAT_len = READ_4_BYTE(-4, -3, -2, -1);
				Param->CINFO = 1 << (((Img->body[Img->ptr + 4] & 0xF0) >> 4) + 8);
				Img->ptr += 6;
				type_enc = Img->body[Img->ptr] & 0x06;
				Param->type = 0;

				if (type_enc == 4) {
					Param->type = 1;
					Img->ptr += Param->IDAT_len + 6;
					check_field(Param, Img);
					Img->ptr -= Param->IDAT_len + 6;
					parse_dunam_huff(Img, Param);
				}

				break;
			}
		}

		Img->ptr++;
	}
}

//TODO: breaks the image
static void png_stego_loop_fix_haff(struct Img_Des *Img, struct Png_Par *Param, struct Line *line) 
{
	uint16_t val;
	uint8_t type, tmp_val1, tmp_val2;
	Img->bit = 3;
	Param->filter = 0;

	while (1) {
		val = read_bit(Img, 7);

		if (Img->bit == 8) {
			Img->bit = 0;
			Img->ptr++;
		}

		if (!val_table_check(val, &type, &tmp_val2)) {
			if (line->bit_set != 8 || line->num < line->str_len) {
				line->str_len = 0;
				fputs("Process was not complete\n", stderr);
			}
			png_do_something_else(Img, line);
		}

		if (tmp_val2) {
			tmp_val1 = Img->bit + tmp_val2;
			if (tmp_val1 > 8) {
				val |= Img->body[Img->ptr++] & 0x80;
				val |= (Img->body[Img->ptr] & 0x01) << 8;
				Img->bit = 1;
			} else {
				val |= (Img->body[Img->ptr] & ((tmp_val2 == 1 ? 0x01 : 0x02) << Img->bit)) << (7 - Img->bit);
				Img->bit = tmp_val1;	
			}
		}

		if (!Param->filter) {
			if (val != 12) {
				error_alert("Unsupported filter png");
			}
			Param->filter++;
		} else if (type > 2) {
			val = convert_7bit(val);
			tmp_val1 = val - 257;
			val = table_len[tmp_val1][1];
			if (val == 255) val += 3;

			if (tmp_val1 > 7 && tmp_val1 < 28) {
				tmp_val1 = table_len[tmp_val1][0];
				tmp_val2 = (uint8_t)read_bit(Img, tmp_val1);

				if (Img->bit == 8) {
					Img->bit = 0;
					Img->ptr++;
				}

				val += tmp_val2;
			}
			Param->sub_len += val;

			tmp_val1 = (uint8_t)read_bit(Img, 5);
			tmp_val1 = convert_5bit(tmp_val1);
			Img->bit += table_shift[tmp_val1];

			if (Img->bit >= 8) {
				Img->bit -= 8;
				Img->ptr++;

				if (Img->bit > 8) {
					Img->bit -= 8;
					Img->ptr++;
				}
			} 
		} else {
			png_do_something(Img, line);

			if (Img->bit == 8) {
				Img->bit = 0;
				Img->ptr++;
			}

			Param->sub_len++;
		}

		if (Param->sub_len == Param->line_val) {
			Param->sub_len = 0;
			Param->filter--;
		}

		if (line->bit_set == 8) {
			line->bit_set = 0;
			png_do_something_else(Img, line);
			line->num++;
		}
	}
}

static void dun_haff_finish(struct Img_Des *Img, struct Line *line)
{
	FILE *img;
	uint32_t hash, i = 0;
	uint8_t buf[Img->size - Img->ptr];
	uint8_t field[line->num + 12];

	hash = adler32(line->str, line->num);
	field[0] = field[1] = field[2] = 0;
	field[3] = line->num;
	field[4] = 0x74;
	field[5] = 0x45;
	field[6] = 0x58;
	field[7] = 0x74;

	for (; i < line->num; i++) {
		field[8 + i] = line->str[i];
	}

	i += 8;

	field[i++] = (hash & 0xFF000000) >> 24;
	field[i++] = (hash & 0x00FF0000) >> 16;
	field[i++] = (hash & 0x0000FF00) >> 8;
	field[i] = (hash & 0x000000FF);

	for (uint16_t j = 0, i = Img->ptr + 1; i <= Img->size; i++, j++) {
		buf[j] = Img->body[i];
	}

	img = fopen(Img->stego_name, "wb");
	fwrite(Img->body, 1, Img->ptr, img);
	fwrite(field, 1, sizeof(field), img);
	fwrite(buf, 1, sizeof(buf), img);
	fclose(img);
	exit(1);
}

static uint16_t find_val_shift(struct Tree *tree_shift, struct Img_Des *Img)
{
	while (!tree_shift->left && !tree_shift->right) {
	 	if (Img->bit >= 8) {
			Img->bit = 0;
			Img->ptr++;
		}

		if (Img->body[Img->ptr] & (0x01 << Img->bit)) {
			tree_shift = tree_shift->right;
		} else {
			tree_shift = tree_shift->left;
		}
		Img->bit++;
	}

	return tree_shift->val;
}
static void png_stego_loop_dun_haff(struct Img_Des *Img, struct Png_Par *Param, struct Line *line)
{
	struct Tree *tree_main, *tree_shift;
	struct Line sub_line;
	uint16_t tmp;
	uint8_t tree_val;

	if (line->status == 1) {
		sub_line.str_len = line->str_len;
	} else if (line->status == 2) {
		if (!Img->body[Param->sub_ptr - 1]) {
			line->str_len = Img->body[Param->sub_ptr];
			Param->sub_ptr += 5;
			line->str = (char *)&Img->body[Param->sub_ptr];
			Param->sub_ptr -= 9;
			sub_line.str_len = 10;
		} else error_alert("Incorrect image 2");
	}

	sub_line.bit_set = sub_line.num = 0;
	sub_line.str = (char *)malloc(sub_line.str_len * sizeof(char));
	sub_line.str[0] = 0;
	tree_main = Param->tree_val;
	tree_shift = Param->tree_shift;

	 while (1) {
	 	if (Img->bit >= 8) {
			Img->bit = 0;
			Img->ptr++;
		}

		if (Img->body[Img->ptr] & (0x01 << Img->bit)) {
			tree_main = tree_main->right;
		} else {
			tree_main = tree_main->left;
		}

		if (!tree_main->left && !tree_main->right) {
			tree_val = (uint8_t)tree_main->val;
			tree_main = Param->tree_val;

			if (tree_val == 256) {
				sub_line.str_len = 0;
				png_do_something_else(Img, &sub_line);
			} else if (tree_val > 256) {
				Img->bit += table_len[tree_val - 257][0];
				tmp = find_val_shift(tree_shift, Img);
				tree_shift = Param->tree_shift;
				Img->bit += table_shift[tmp];
			} else {
				png_do_something_with_this(line, &sub_line, tree_val);

				if (sub_line.bit_set == 8) {
					sub_line.bit_set = 0;
					sub_line.num++;
					if (sub_line.num == sub_line.str_len) {
						sub_line.str_len += 10;
						sub_line.str = (char *)realloc(sub_line.str, sizeof(char) * sub_line.str_len);
					}
					sub_line.str[sub_line.num] = 0;
				}

				if (line->bit_set == 8) {
					line->bit_set = 0;
					line->num++;
					if (line->num == line->str_len) {
						if (line->status == 1)
							Img->ptr = Param->sub_ptr;
						sub_line.str_len = 0;
						png_do_something_else(Img, &sub_line);
					}
				}
			}
		}

		Img->bit++;
	 }
}

void png_start_stego(struct Img_Des *Img, struct Start_Par *Str_Par)
{
	struct Png_Par Param;
	struct Line line;
	uint8_t bit_fill[] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

	Img->help_read = bit_fill;
	Img->bit = Img->ptr = Param.sub_len = line.num = line.bit_set = 0;
	line.str = NULL;

	png_parse_head(Img, &Param);

	if (Str_Par->action == 1) {
		get_user_stirng(&line);
		if (line.str == NULL) error_alert("Incorrect input");
		line.status = 1;
		Img->stego_name = get_stego_name(Str_Par);
		if (!Param.type) {
			png_do_something = stego_write;
			png_do_something_else = img_add_write;
			png_stego_loop_fix_haff(Img, &Param, &line);
		} else {
			png_do_something_else = dun_haff_finish;
			png_do_something_with_this = dun_stego_write;
			png_stego_loop_dun_haff(Img, &Param, &line);
		}
	} else if (Str_Par->action == 2) {
		line.status = 2;
		png_do_something_else = img_add_read;
		if (!Param.type) {
			line.str_len = 10;
			line.str = (char *)malloc(10 * sizeof(char));
			png_do_something = stego_read;
			png_stego_loop_fix_haff(Img, &Param, &line);
		} else {
			if (Param.sub_len) error_alert("Incorrect image 1");
			png_do_something_with_this = dun_stego_read;
			png_stego_loop_dun_haff(Img, &Param, &line);
		}
	}
}
