#ifndef HELPER
#define HELPER

struct Start_Par
{
	char *file_name;
	uint8_t type;
	uint8_t action;
	uint8_t act_add;
};

struct Line {
	char *str;
	uint8_t str_len;
	uint8_t num;
	uint8_t status;
	uint8_t bit_set;
};

struct Img_Des
{
	char *stego_name;
	uint8_t *body;
	uint32_t ptr;
	uint32_t size;
	uint8_t bit;
};

uint8_t check_exp(FILE *);
void error_alert(char *);
void get_user_stirng(struct Line *);
char* get_stego_name(struct Start_Par *);
void parse_comnd_line(int, char **, struct Start_Par *);
void img_add_read(struct Img_Des *, struct Line *);
void img_add_write(struct Img_Des *, struct Line *);
void stego_write(struct Img_Des *, struct Line *);
void stego_read(struct Img_Des *, struct Line *);

#endif