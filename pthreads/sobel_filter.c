#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define MIN(a, b) (a < b? a : b)

typedef struct {
    unsigned char **pixels;
    int height;
    int width;
    unsigned char max_value;
} image;

typedef struct {
    image *in;
    image *out;
    int thread_id;
} thread_struct;

int num_threads;
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

void *thread_function(void *var) {
    thread_struct thread = *(thread_struct*) var;
    int i, j, x, y, low, high, interval_size;
    float gx_sum, gy_sum;

    interval_size = ceil ((float) (thread.out -> height) / num_threads);
    low = thread.thread_id * interval_size;
    low = low == 0 ? 1 : low;
    high = MIN ((thread.thread_id + 1) * interval_size, thread.in -> height);

    for (i = low; i < high - 1; i++) {
        for (j = 1; j < thread.in -> width - 1; j++) {
            gx_sum = 0.0;
            gy_sum = 0.0;
            for (x = i - 1; x < i + 2; x++) {
                for (y = j - 1; y < j + 2; y++) {
                    gx_sum += (thread.in->pixels[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                    gy_sum += (thread.in->pixels[x][y] * Gy_sobel[x - i + 1][y - j + 1]);
                }
            }
            thread.out->pixels[i][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));
        }
    }
}

void sobel(image *in, image *out) {
    int i;
    pthread_t tid[num_threads];
    thread_struct *thread = malloc (num_threads * sizeof(thread_struct));

    out->height = in->height;
    out->width = in->width;
    out->max_value = in->max_value;

    out->pixels = allocation_bw (out->height, out->width);

    for (i = 0; i < num_threads; i++) {
        thread[i].in = in;
        thread[i].thread_id = i;
        thread[i].out = out;
    }

    for (i = 0; i < num_threads; i++) {
        pthread_create(&(tid[i]), NULL, thread_function, &(thread[i]));
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(tid[i], NULL);
    }

    for (i = 0; i < num_threads; i++) {
        out = thread[i].out;
    }
}

int main(int argc, char *argv[]) {
    //argv[1] input
	//argv[2] output
	//argv[3] num_threads

    if(argc < 4) {
		printf("Incorrect number of arguments\n");
		exit(-1);
	}

	num_threads = atoi(argv[3]);
    image input;
    image output;

    read_image(argv[1], &input);
    sobel(&input, &output);
    write_image(argv[2], &output);

    return 0;
}