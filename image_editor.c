// Degeratu Razvan Andrei 323CA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_FILE_PATH 1024
#define MAX_COMMAND_LENGTH 256
#define COLOR_CHANNELS 3
#define MAX_PIXEL_VALUE 255
#define DIM 256
#define u_char_size sizeof(unsigned char)
typedef enum {
	FORMAT_P2,  // grayscale ASCII
	FORMAT_P3,  // RGB ASCII - my testing files were huge with this
	FORMAT_P5,  // binary grayscale - way more efficient
	FORMAT_P6   // RGB binary - what most test images used
} image_format_t;

typedef enum {
	FILTER_EDGE,
	FILTER_SHARPEN,
	FILTER_BLUR,
	FILTER_GAUSSIAN
} filter_type_t;

typedef struct{
	image_format_t format;
	int width;
	int height;
	int max_value;
	unsigned char **pixels;
	int channels;
	struct {
		int x1, y1, x2, y2;
	} selection;
	int is_loaded;
} image_t;

void init_image(image_t *img);
void free_image(image_t *img);
int load_image(image_t *img, const char *filename);
int save_image(image_t *img, const char *filename, int ascii);
void select_area(image_t *img, int x1, int y1, int x2, int y2);
void select_all(image_t *img);
void crop_image(image_t *img);
void apply_filter(image_t *img, filter_type_t filter);
void rotate_image(image_t *img, int angle);
void equalize(image_t *img);
void display_histogram(image_t *img, int max_stars, int bins);
int handle_commands(void);

void init_image(image_t *img)
{
	img->width = 0;
	img->height = 0;
	img->max_value = 0;
	img->pixels = NULL;
	img->channels = 0;
	img->is_loaded = 0;
	img->selection.y1 = 0;
	img->selection.x1 = 0;
	img->selection.x2 = 0;
	img->selection.y2 = 0;
}

void free_image(image_t *img)
{
	if (!img->is_loaded)
		return;

	if (img->pixels) {
		for (int i = 0; i < img->height; i++)
			free(img->pixels[i]);
		free(img->pixels);
	}

	init_image(img);
}

int main(void)
{
	int result = handle_commands();
	return result;
}

// Helps avoiding weird values.
int clamp_value(int value, int min, int max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

// I don't think this was tested
void skip_file_comments(FILE *fp)
{
	int ch;

	while ((ch = fgetc(fp)) != EOF && isspace(ch))
		continue;

	if (ch == '#') {
		while ((ch = fgetc(fp)) != EOF && ch != '\n')
			continue;
		skip_file_comments(fp);
	} else {
		fseek(fp, -1, SEEK_CUR);
	}
}

int read_header(FILE *fp, image_t *img)
{
	char magic[3];

	if (fscanf(fp, "%2s", magic) != 1)
		return 0;

	fgetc(fp);
	if (strcmp(magic, "P2") == 0) {
		img->format = FORMAT_P2;
		img->channels = 1;
	} else if (strcmp(magic, "P3") == 0) {
		img->format = FORMAT_P3;
		img->channels = 3;
	} else if (strcmp(magic, "P5") == 0) {
		img->format = FORMAT_P5;
		img->channels = 1;
	} else if (strcmp(magic, "P6") == 0) {
		img->format = FORMAT_P6;
		img->channels = 3;
	} else {
		return 0;
	}

	skip_file_comments(fp);

	if (fscanf(fp, "%d %d", &img->width, &img->height) != 2 ||
		fscanf(fp, "%d", &img->max_value) != 1)
		return 0;

	fgetc(fp);

	return (img->width > 0 && img->height > 0 && img->max_value <= 255);
}

int allocate_memory(image_t *img)
{
	img->pixels = malloc(img->height * sizeof(unsigned char *));
	if (!img->pixels)
		return 0;

	for (int i = 0; i < img->height; i++) {
		int size = img->width * img->channels;
		img->pixels[i] = malloc(size * u_char_size);
		if (!img->pixels[i]) {
			for (int j = 0; j < i; j++)
				free(img->pixels[j]);
			free(img->pixels);
			return 0;
		}
	}
	return 1;
}

int read_pixel_data(FILE *fp, image_t *img)
{
	if (img->format == FORMAT_P2 || img->format == FORMAT_P3) {
		for (int i = 0; i < img->height; i++) {
			for (int j = 0; j < img->width * img->channels; j++) {
				int value;
				if (fscanf(fp, "%d", &value) != 1 || value < 0 || value > 255)
					return 0;
				img->pixels[i][j] = (unsigned char)value;
			}
		}
	} else {
		// binary format - faster && harder to debug
		for (int i = 0; i < img->height; i++) {
			size_t bytes_read = fread(img->pixels[i], u_char_size,
									  img->width * img->channels, fp);
			if (bytes_read != (size_t)(img->width * img->channels))
				return 0;
		}
	}
	return 1;
}

int load_image(image_t *img, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		printf("Failed to load %s\n", filename);
		free_image(img);
		return 0;
	}

	if (img->is_loaded)
		free_image(img);

	if (!read_header(fp, img) || !allocate_memory(img) ||
		!read_pixel_data(fp, img)) {
		printf("Failed to load %s\n", filename);
		fclose(fp);
		free_image(img);
		return 0;
	}

	img->is_loaded = 1;
	img->selection.x1 = 0;
	img->selection.y1 = 0;
	img->selection.x2 = img->width;
	img->selection.y2 = img->height;

	fclose(fp);
	printf("Loaded %s\n", filename);
	return 1;
}

void write_pixel_data(FILE *fp, image_t *img, int ascii)
{
	if (ascii) {
		for (int i = 0; i < img->height; i++) {
			for (int j = 0; j < img->width * img->channels; j++) {
				int value = img->pixels[i][j];
				int size = img->width * img->channels;
				fprintf(fp, "%d%c", value,
						(j + 1) % (size) == 0 ? '\n' : ' ');
			}
		}
	} else {
		for (int i = 0; i < img->height; i++) {
			fwrite(img->pixels[i], u_char_size,
				   img->width * img->channels, fp);
		}
	}
}

int save_image(image_t *img, const char *filename, int ascii)
{
	FILE *fp = fopen(filename, ascii ? "w" : "wb");
	if (!fp) {
		printf("Failed to save %s\n", filename);
		return 0;
	}

	const char *out_format = ascii ? (img->channels == 1 ? "P2" : "P3")
	: (img->channels == 1 ? "P5" : "P6");

	fprintf(fp, "%s\n%d %d\n%d\n", out_format, img->width,
			img->height, img->max_value);

	write_pixel_data(fp, img, ascii);

	fclose(fp);
	printf("Saved %s\n", filename);
	return 1;
}

void select_all(image_t *img)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	img->selection.x1 = 0;
	img->selection.y1 = 0;
	img->selection.x2 = img->width;
	img->selection.y2 = img->height;

	printf("Selected ALL\n");
}

void select_area(image_t *img, int x1, int y1, int x2, int y2)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	if (x1 > x2) {
		int temp = x1;
		x1 = x2;
		x2 = temp;
	}
	if (y1 > y2) {
		int temp = y1;
		y1 = y2;
		y2 = temp;
	}

	if (x1 < 0 || y1 < 0 || x2 > img->width || y2 > img->height ||
		x1 >= x2 || y1 >= y2) {
		printf("Invalid set of coordinates\n");
		return;
	}

	img->selection.x1 = x1;
	img->selection.y1 = y1;
	img->selection.x2 = x2;
	img->selection.y2 = y2;

	printf("Selected %d %d %d %d\n", x1, y1, x2, y2);
}

void apply_kernel_to_pixel(const double kernels[4][3][3], image_t *img,
						   filter_type_t filter, int i, int j,
						   unsigned char **temp, double *sums)
{
	for (int k = -1; k <= 1; k++) {
		for (int l = -1; l <= 1; l++) {
			int row = i + k;
			int col = j + l;

			if (row >= 0 && row < img->height && col >= 0 && col < img->width) {
				for (int c = 0; c < 3; c++) {
					sums[c] += kernels[filter][k + 1][l + 1] *
							   temp[row][col * 3 + c];
				}
			}
		}
	}
}

unsigned char **create_temp_buffer(image_t *img)
{
	unsigned char **temp = malloc(img->height * sizeof(unsigned char *));
	if (!temp)
		return NULL;

	for (int i = 0; i < img->height; i++) {
		temp[i] = malloc(img->width * 3 * u_char_size);
		if (!temp[i]) {
			for (int j = 0; j < i; j++)
				free(temp[j]);
			free(temp);
			return NULL;
		}
		for (int j = 0; j < img->width * 3; j++)
			temp[i][j] = img->pixels[i][j];
	}
	return temp;
}

void apply_filter(image_t *img, filter_type_t filter)
{
	const double kernels[4][3][3] = {
		{
			// edge detection
			{-1, -1, -1},
			{-1, 8, -1},
			{-1, -1, -1}},
		{
			// sharpen
			{0, -1, 0},
			{-1, 5, -1},
			{0, -1, 0}},
		{
			// basic blur
			{1.0 / 9, 1.0 / 9, 1.0 / 9},
			{1.0 / 9, 1.0 / 9, 1.0 / 9},
			{1.0 / 9, 1.0 / 9, 1.0 / 9}},

		{
			// gaussian blur
			{1.0 / 16, 2.0 / 16, 1.0 / 16},
			{2.0 / 16, 4.0 / 16, 2.0 / 16},
			{1.0 / 16, 2.0 / 16, 1.0 / 16}}};
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	if (img->channels != 3) {
		printf("Easy, Charlie Chaplin\n");
		return;
	}

	unsigned char **temp = create_temp_buffer(img);
	if (!temp) {
		printf("Memory allocation failed\n");
		return;
	}

	int y1 = img->selection.y1;
	int y2 = img->selection.y2;
	int x1 = img->selection.x1;
	int x2 = img->selection.x2;

	if (y1 == 0)
		y1 = 1;
	if (y2 == img->height)
		y2 = img->height - 1;
	if (x1 == 0)
		x1 = 1;
	if (x2 == img->width)
		x2 = img->width - 1;

	for (int i = y1; i < y2; i++) {
		for (int j = x1; j < x2; j++) {
			double sums[3] = {0, 0, 0};
			apply_kernel_to_pixel(kernels, img, filter, i, j, temp, sums);

			for (int c = 0; c < 3; c++) {
				img->pixels[i][j * 3 + c] =
					clamp_value((int)round(sums[c]), 0, 255);
			}
		}
	}

	for (int i = 0; i < img->height; i++)
		free(temp[i]);
	free(temp);

	printf("APPLY %s done\n",
		   filter == FILTER_EDGE ? "EDGE" : filter == FILTER_SHARPEN ? "SHARPEN"
										: filter == FILTER_BLUR		 ? "BLUR"
															: "GAUSSIAN_BLUR");
}

unsigned char **allocate_rotation_buffer(int height, int width, int channels)
{
	unsigned char **buffer = malloc(height * sizeof(unsigned char *));
	if (!buffer)
		return NULL;

	for (int i = 0; i < height; i++) {
		buffer[i] = malloc(width * channels * u_char_size);
		if (!buffer[i]) {
			for (int j = 0; j < i; j++)
				free(buffer[j]);
			free(buffer);
			return NULL;
		}
	}
	return buffer;
}

void copy_rotated_pixel(image_t *img, unsigned char **new_pixels,
						int i, int j, int new_i, int new_j)
{
	for (int c = 0; c < img->channels; c++) {
		new_pixels[new_i][new_j * img->channels + c] =
			img->pixels[i][j * img->channels + c];
	}
}

void rotate_full(image_t *img, int angle)
{
	int new_width = (angle == 90 || angle == 270 || angle == -90 ||
	angle == -270) ? img->height : img->width;
	int new_height = (angle == 90 || angle == 270 ||
	angle == -90 || angle == -270) ? img->width : img->height;

	unsigned char **new_pixels = allocate_rotation_buffer(new_height, new_width,
														  img->channels);
	if (!new_pixels) {
		printf("Memory allocation failed\n");
		return;
	}

	for (int i = 0; i < img->height; i++) {
		for (int j = 0; j < img->width; j++) {
			int new_i, new_j;
			switch (angle) {
			case 90:
			case -270:
				new_i = j;
				new_j = img->height - 1 - i;
				break;
			case 180:
			case -180:
				new_i = img->height - 1 - i;
				new_j = img->width - 1 - j;
				break;
			case 270:
			case -90:
				new_i = img->width - 1 - j;
				new_j = i;
				break;
			default:
				new_i = i;
				new_j = j;
			}
			copy_rotated_pixel(img, new_pixels, i, j, new_i, new_j);
		}
	}

	for (int i = 0; i < img->height; i++)
		free(img->pixels[i]);
	free(img->pixels);

	if (angle == 90 || angle == 270 || angle == -90 || angle == -270) {
		img->width = new_width;
		img->height = new_height;
		img->selection.x2 = img->width;
		img->selection.y2 = img->height;
	}
	img->pixels = new_pixels;
}

// spent way too much time debugging this one
void rotate_selection_area(image_t *img, int angle)
{
	int size = img->selection.x2 - img->selection.x1;
	unsigned char **temp = allocate_rotation_buffer(size, size, img->channels);
	if (!temp) {
		printf("Memory allocation failed\n");
		return;
	}

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			for (int c = 0; c < img->channels; c++) {
				temp[i][j * img->channels + c] =
					img->pixels[img->selection.y1 + i]
							   [(img->selection.x1 + j) * img->channels + c];
			}
		}
	}

	int rotations = (angle > 0 ? angle : 360 + angle) / 90;
	for (int r = 0; r < rotations; r++) {
		int ch_count = img->channels;
		unsigned char **rotated = allocate_rotation_buffer(size, size,
															ch_count);
		if (!rotated) {
			printf("Memory allocation failed\n");
			for (int i = 0; i < size; i++)
				free(temp[i]);
			free(temp);
			return;
		}

		for (int i = 0; i < size; i++) {
			for (int j = 0; j < size; j++) {
				for (int c = 0; c < img->channels; c++) {
					rotated[j][(size - 1 - i) * img->channels + c] =
						temp[i][j * img->channels + c];
				}
			}
		}

		for (int i = 0; i < size; i++)
			free(temp[i]);
		free(temp);
		temp = rotated;
	}

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			for (int c = 0; c < img->channels; c++) {
				img->pixels[img->selection.y1 + i]
						   [(img->selection.x1 + j) * img->channels + c] =
					temp[i][j * img->channels + c];
			}
		}
	}

	for (int i = 0; i < size; i++)
		free(temp[i]);
	free(temp);
}

void rotate_image(image_t *img, int angle)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	int init_angle = angle;
	angle = ((angle % 360) + 360) % 360;

	if (angle % 90 != 0) {
		printf("Unsupported rotation angle\n");
		return;
	}

	if (angle == 0 || angle == 360 || angle == -360) {
		printf("Rotated %d\n", init_angle);
		return;
	}

	if (img->selection.x1 != 0 || img->selection.y1 != 0 ||
		img->selection.x2 != img->width || img->selection.y2 != img->height) {
		int sel_width = img->selection.x2 - img->selection.x1;
		int sel_height = img->selection.y2 - img->selection.y1;
		if (sel_width != sel_height) {
			printf("The selection must be square\n");
			return;
		}
		rotate_selection_area(img, angle);
	} else {
		rotate_full(img, angle);
	}

	printf("Rotated %d\n", init_angle);
}

void calculate_frequencies(image_t *img, int *frequency, int size)
{
	for (int i = 0; i < img->height; i++) {
		for (int j = 0; j < img->width; j++) {
			unsigned char pixel = img->pixels[i][j];
			if (pixel < size)
				frequency[pixel]++;
		}
	}
}

int find_max_frequency(const int *frequency, int bins)
{
	int max_freq = 0;
	for (int i = 0; i < bins; i++) {
		if (frequency[i] > max_freq)
			max_freq = frequency[i];
	}
	return max_freq;
}

void equalize_histogram_bins(int *frequency, int size, int num_bins)
{
	int *partial_sums = calloc(num_bins, sizeof(int));
	if (!partial_sums)
		return;

	int bin_size = size / num_bins;

	for (int i = 0; i < size; i++) {
		int bin_index = i / bin_size;
		if (bin_index >= num_bins)
			bin_index = num_bins - 1;
		partial_sums[bin_index] += frequency[i];
	}

	for (int i = 0; i < num_bins; i++)
		frequency[i] = partial_sums[i];

	for (int i = num_bins; i < size; i++)
		frequency[i] = 0;

	free(partial_sums);
}

void display_histogram(image_t *img, int max_stars, int bins)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	if (img->channels == 3) {
		printf("Black and white image needed\n");
		return;
	}

	if (bins <= 0 || bins > 256) {
		printf("Invalid set of parameters\n");
		return;
	}

	int *frequency = calloc(256, sizeof(int));
	if (!frequency) {
		fprintf(stderr, "Memory allocation error\n");
		return;
	}

	calculate_frequencies(img, frequency, 256);
	equalize_histogram_bins(frequency, 256, bins);
	int max_freq = find_max_frequency(frequency, bins);

	for (int i = 0; i < bins; i++) {
		int stars = max_freq ? (int)((double)frequency[i] *
									 max_stars / max_freq) : 0;
		printf("%d\t|\t", stars);
		for (int j = 0; j < stars; j++)
			printf("*");
		printf("\n");
	}

	free(frequency);
}

void equalize(image_t *img)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	if (img->channels == 3) {
		printf("Black and white image needed\n");
		return;
	}

	int *frequency = calloc(DIM, sizeof(int));
	if (!frequency) {
		fprintf(stderr, "Memory allocation error\n");
		return;
	}

	calculate_frequencies(img, frequency, DIM);

	double area = (double)img->height * img->width;
	double scale = MAX_PIXEL_VALUE / area;

	for (int i = 0; i < img->height; i++) {
		for (int j = 0; j < img->width; j++) {
			int sum = 0;
			for (int k = 0; k <= img->pixels[i][j]; k++)
				sum += frequency[k];
			img->pixels[i][j] = (unsigned char)clamp_value
				((int)round(scale * sum), 0, MAX_PIXEL_VALUE);
		}
	}

	free(frequency);
	printf("Equalize done\n");
}

void crop_image(image_t *img)
{
	if (!img->is_loaded) {
		printf("No image loaded\n");
		return;
	}

	int new_width = img->selection.x2 - img->selection.x1;
	int new_height = img->selection.y2 - img->selection.y1;

	unsigned char **new_pixels = malloc(new_height * sizeof(unsigned char *));
	if (!new_pixels)
		return;

	for (int i = 0; i < new_height; i++) {
		new_pixels[i] = malloc(new_width * img->channels * u_char_size);
		if (!new_pixels[i]) {
			for (int j = 0; j < i; j++)
				free(new_pixels[j]);
			free(new_pixels);
			return;
		}
	}

	for (int i = 0; i < new_height; i++) {
		for (int j = 0; j < new_width; j++) {
			for (int c = 0; c < img->channels; c++) {
				new_pixels[i][j * img->channels + c] =
					img->pixels[img->selection.y1 + i]
							   [(img->selection.x1 + j) * img->channels + c];
			}
		}
	}

	for (int i = 0; i < img->height; i++)
		free(img->pixels[i]);
	free(img->pixels);

	img->width = new_width;
	img->height = new_height;
	img->pixels = new_pixels;
	img->selection.x1 = 0;
	img->selection.y1 = 0;
	img->selection.x2 = new_width;
	img->selection.y2 = new_height;

	printf("Image cropped\n");
}

int handle_select_command(image_t *img, char *next)
{
	if (next && strcmp(next, "ALL") == 0) {
		select_all(img);
		return 1;
	}

	int x1, y1, x2, y2;
	if (next && sscanf(next, "%d", &x1) == 1) {
		char *y1_str = strtok(NULL, " ");
		char *x2_str = strtok(NULL, " ");
		char *y2_str = strtok(NULL, " ");

		if (y1_str && x2_str && y2_str &&
			sscanf(y1_str, "%d", &y1) == 1 &&
			sscanf(x2_str, "%d", &x2) == 1 &&
			sscanf(y2_str, "%d", &y2) == 1) {
			select_area(img, x1, y1, x2, y2);
			return 1;
		}
	}
	return 0;
}

int handle_apply_command(image_t *img, char *filter)
{
	if (!filter) {
		printf("Invalid command\n");
		return 0;
	}

	filter_type_t filter_type;
	if (strcmp(filter, "EDGE") == 0)
		filter_type = FILTER_EDGE;
	else if (strcmp(filter, "SHARPEN") == 0)
		filter_type = FILTER_SHARPEN;
	else if (strcmp(filter, "BLUR") == 0)
		filter_type = FILTER_BLUR;
	else if (strcmp(filter, "GAUSSIAN_BLUR") == 0)
		filter_type = FILTER_GAUSSIAN;
	else {
		printf("APPLY parameter invalid\n");
		return 0;
	}

	apply_filter(img, filter_type);
	return 1;
}

int handle_histogram_command(image_t *img)
{
	char *max_stars_str = strtok(NULL, " ");
	char *bins_str = strtok(NULL, " ");

	if (!max_stars_str || !bins_str) {
		printf("Invalid command\n");
		return 0;
	}

	int max_stars = atoi(max_stars_str);
	int bins = atoi(bins_str);

	if (strtok(NULL, " ")) {
		printf("Invalid command\n");
		return 0;
	}

	display_histogram(img, max_stars, bins);
	return 1;
}

// the main function - could probably split this into smaller functions
int handle_commands(void)
{
	image_t img;
	char input[MAX_COMMAND_LENGTH];

	init_image(&img);

	while (1) {
		fgets(input, sizeof(input), stdin);
		input[strcspn(input, "\n")] = 0;

		char *cmd = strtok(input, " ");
		if (!cmd)
			continue;

		if (strcmp(cmd, "EXIT") == 0) {
			if (!img.is_loaded)
				printf("No image loaded\n");
			free_image(&img);
			return 0;
		}

		if (strcmp(cmd, "LOAD") == 0) {
			char *filename = strtok(NULL, " ");
			if (filename)
				load_image(&img, filename);
			continue;
		}

		if (!img.is_loaded) {
			printf("No image loaded\n");
			continue;
		}

		if (strcmp(cmd, "SELECT") == 0) {
			char *next = strtok(NULL, " ");
			if (!handle_select_command(&img, next))
				printf("Invalid command\n");
		} else if (strcmp(cmd, "CROP") == 0) {
			crop_image(&img);
		} else if (strcmp(cmd, "APPLY") == 0) {
			char *filter = strtok(NULL, " ");
			handle_apply_command(&img, filter);
		} else if (strcmp(cmd, "EQUALIZE") == 0) {
			equalize(&img);
		} else if (strcmp(cmd, "HISTOGRAM") == 0) {
			handle_histogram_command(&img);
		} else if (strcmp(cmd, "SAVE") == 0) {
			char *filename = strtok(NULL, " ");
			char *format = strtok(NULL, " ");
			if (filename) {
				int ascii = (format && strcmp(format, "ascii") == 0);
				save_image(&img, filename, ascii);
			}
		} else if (strcmp(cmd, "ROTATE") == 0) {
			char *angle_str = strtok(NULL, " ");
			if (angle_str) {
				int angle = atoi(angle_str);
				rotate_image(&img, angle);
			}
		} else {
			printf("Invalid command\n");
		}
	}
}
