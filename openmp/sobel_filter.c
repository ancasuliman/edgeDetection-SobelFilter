#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

typedef struct {
    unsigned char **pixels;
    int height;
    int width;
    unsigned char max_value;
} image;

int Gx_sobel[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
int Gy_sobel[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};

void read_image(const char *name, image *img) {
    FILE *image_file;
	char newline, image_type[3];
    unsigned char *buffer;
	int width, height, max_value, i, j, offset; 
	
    image_file = fopen(name, "rb");
    // skip image type
    fgets(image_type, sizeof(image_type), image_file);
	fscanf(image_file, "%d %d", &width, &height);

	img->height = height;
	img->width = width;

    unsigned char *pixels = (unsigned char*) malloc(height * width * sizeof(unsigned char));
    img->pixels = (unsigned char **) malloc(height * sizeof(unsigned char*));
    for(i = 0; i < height; i++) {
        img->pixels[i] = &(pixels[width * i]);
    }

	fscanf(image_file, "%d", &max_value);
	fscanf(image_file, "%c", &newline);
	
	img->max_value = max_value;

    buffer = (unsigned char *) malloc(height * width * sizeof(unsigned char));
    fread(buffer, sizeof(unsigned char), height * width, image_file);
    
    offset = 0;
    for(i = 0; i < height; i++) {
        for(j = 0; j < width; j++) {
            img->pixels[i][j] = buffer[offset];
            offset++;
        }
    }

	fclose(image_file);
}

void write_image(const char *name, image *img) {
    FILE *image_file;
    unsigned char buffer[img->height * img->width];
    int offset, i, j;

	image_file = fopen(name, "wb");
    
    // grayscale .pgm image
    fprintf(image_file, "P5\n");
    fprintf(image_file, "%d %d\n", img->width, img->height);
   
    fprintf(image_file, "%d\n", img->max_value);
    
    offset = 0;
    for (i = 0; i < img->height; i++) {
        for(j = 0; j < img->width; j++) {
            buffer[offset] = img->pixels[i][j];
            offset++;		
        }
    }

    fwrite(buffer, sizeof(buffer), 1, image_file);	

	fclose(image_file);
}

unsigned char **allocation_bw(int height, int width) {
    int i, j;
    unsigned char **img;
    
    img = malloc (height * sizeof(unsigned char *));
    if (img == NULL) 
        return NULL;

    for (i = 0; i < height; i++) {
        img[i] = malloc (width * sizeof(unsigned char));
    }

    return img;
}

void sobel(image *in, image *out) {
    int i, j, x, y;
    float gx_sum, gy_sum;

    out->height = in->height;
    out->width = in->width;
    out->max_value = in->max_value;

    out->pixels = allocation_bw (out->height, out->width);
    
    #pragma omp parallel for
    for (i = 1; i < in->height - 1; i++) {
        for (j = 1; j < in->width - 1; j++) {
            gx_sum = 0.0;
            gy_sum = 0.0;
            for (x = i - 1; x < i + 2; x++) {
                for (y = j - 1; y < j + 2; y++) {
                    gx_sum += (in->pixels[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                    gy_sum += (in->pixels[x][y] * Gy_sobel[x - i + 1][y - j + 1]);
                }
            }
            out->pixels[i][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));
        }
    }
    
}

int main(int argc, char *argv[]) {
    //argv[1] input
	//argv[2] output
    if(argc < 3) {
		printf("Incorrect number of arguments\n");
		exit(-1);
	}

    image input;
    image output;

    read_image(argv[1], &input);
    sobel(&input, &output);
    write_image(argv[2], &output);

    return 0;
}