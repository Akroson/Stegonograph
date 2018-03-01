#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "helper.h"
#include "png_stego.h"
#include "jpeg_stego.h"

static void (*start_stego(uint8_t type))(struct Img_Des *, struct Start_Par *)
{
	switch (type) {
		case 1:
			return jpeg_start_stego;
		case 2:
			return png_start_stego;
	}
}

int main(int argc, char *argv[])
{
	struct Start_Par Par;
	struct Img_Des Img;
	uint32_t result;

	parse_comnd_line(argc, argv, &Par);
	FILE *img = fopen(Par.file_name, "rb");
	
	if (img == NULL) 
		error_alert("Cant open file");

	Par.type = check_exp(img);
	if (!Par.type) {
		fclose(img);
		error_alert("Incorrect or unsupported format");
	} else {
		fseek(img, 0, SEEK_END);
		Img.size = ftell(img);
		rewind(img);

		Img.body = (uint8_t *)malloc(sizeof(uint8_t) * Img.size);
		
		result = fread(Img.body, 1, Img.size, img);
		fclose(img);

		if (result != Img.size) 
			error_alert("Error read");
	}

	start_stego(Par.type)(&Img, &Par);

	return 0;
}