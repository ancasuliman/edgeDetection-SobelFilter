#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

typedef struct {
    unsigned char **pixels;
    int height;
    int width;
    unsigned char max_value;
} image;

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

int main(int argc, char **argv) {
    image img;
    int i, j, x, y, width, height, rank, N, rows, remainder;
    int Gx_sobel[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy_sobel[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};
    float gx_sum, gy_sum;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &N);

    if (rank == 0) {
        read_image(argv[1], &img);
        rows = img.height / N;

        if (N != 1) {
            for (i = rows; i < rows * N; i += rows) {
	    		MPI_Send(&(img.height), 1, MPI_INT, i / rows, 0, MPI_COMM_WORLD);
	    		MPI_Send(&(img.width), 1, MPI_INT, i / rows, 0, MPI_COMM_WORLD); 
            }

            remainder = rows + (img.height % N);
            
            unsigned char mat[img.height][img.width];
            
            memcpy(&(mat[0][0]), &(img.pixels[0][0]), (rows + 1) * img.width);  	
            for(i = rows; i < rows * (N- 1); i += rows) {		    		
                MPI_Send(&(img.pixels[i][0]), rows * img.width, MPI_UNSIGNED_CHAR, i / rows, 0, MPI_COMM_WORLD);		    		
            }
            MPI_Send(&(img.pixels[rows * (N - 1)][0]), remainder * img.width, MPI_UNSIGNED_CHAR, N - 1, 0, MPI_COMM_WORLD);

            MPI_Send(&img.pixels[rows - 1][0], img.width, MPI_UNSIGNED_CHAR, rank + 1, 0, MPI_COMM_WORLD);	    		
            MPI_Recv(&img.pixels[rows][0], img.width, MPI_UNSIGNED_CHAR, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (i = 1; i < rows; i++) {
                for (j = 1; j < img.width - 1; j++) {
                    gx_sum = 0;
                    gy_sum = 0;

                    for (x = i - 1; x < i + 2; x++) {
                        for (y = j - 1; y < j + 2; y++) {
                            gx_sum += (mat[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                            gy_sum += (mat[x][y] * Gy_sobel[x - i + 1][y - j + 1]);        
                        }
                    }

                    img.pixels[i][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));
                }
            }
            for(i = 1; i < N - 1; i++) {
                MPI_Recv(&(img.pixels[rows * i][0]), rows * img.width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            MPI_Recv(&(img.pixels[rows * (N - 1)][0]), remainder * img.width, MPI_UNSIGNED_CHAR, (N - 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            unsigned char mat[img.height][img.width];

            memcpy(&(mat[0][0]), &(img.pixels[0][0]), img.height * img.width);
            for (i = 1; i < img.height - 1; i++) {
                for (j = 1; j < img.width - 1; j++) {
                    gx_sum = 0.0;
                    gy_sum = 0.0;

                    for (x = i - 1; x < i + 2; x++) {
                        for (y = j - 1; y < j + 2; y++) {
                            gx_sum += (mat[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                            gy_sum += (mat[x][y] * Gy_sobel[x - i + 1][y - j + 1]);
                        }
                    }
                    
                    img.pixels[i][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));
                    
                }
            }
        }
        write_image(argv[2], &img);        
    } else if (rank == N - 1) {
    	MPI_Recv(&height, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    	MPI_Recv(&width, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    	rows = height / N + width % N;

        unsigned char mat[rows + 1][width], modified[rows][width];
        MPI_Recv(&(mat[1][0]), rows * width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        memcpy(&(modified[0][0]), &(mat[1][0]), rows * width);

        MPI_Recv(&(mat[0][0]), width, MPI_UNSIGNED_CHAR, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	        	    		
        MPI_Send(&(mat[1][0]), width, MPI_UNSIGNED_CHAR, rank - 1, 0, MPI_COMM_WORLD);

        for (i = 1; i < rows; i++) {
            for (j = 1; j < width - 1; j++) {
                gx_sum = 0;
                gy_sum = 0;

                for (x = i - 1; x < i + 2; x++) {
                    for (y = j - 1; y < j + 2; y++) {
                        gx_sum += (mat[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                        gy_sum += (mat[x][y] * Gy_sobel[x - i + 1][y - j + 1]);        
                    }
                }

                modified[i - 1][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));
            }
        }
        MPI_Send(&(modified[0][0]), rows * width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    } else {
        unsigned char mat[rows + 2][width], modified[rows][width];
        MPI_Recv(&(mat[1][0]), rows * width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        memcpy(&(modified[0][0]), &(mat[1][0]), rows * width);

        MPI_Send(&(mat[rows][0]), width, MPI_UNSIGNED_CHAR, rank + 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&(mat[0][0]), width, MPI_UNSIGNED_CHAR, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	        	    		
        MPI_Send(&(mat[1][0]), width, MPI_UNSIGNED_CHAR, rank - 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&(mat[rows + 1][0]), width, MPI_UNSIGNED_CHAR, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	        	    		

        for (i = 1; i < rows + 1; i++) {
            for (j = 1; j < width - 1; j++) {
                gx_sum = 0;
                gy_sum = 0;

                for (x = i - 1; x < i + 2; x++) {
                    for (y = j - 1; y < j + 2; y++) {
                        gx_sum += (mat[x][y] * Gx_sobel[x - i + 1][y - j + 1]);
                        gy_sum += (mat[x][y] * Gy_sobel[x - i + 1][y - j + 1]);        
                    }
                }

                modified[i - 1][j] = (unsigned char) sqrt(pow(gx_sum, 2) + pow(gy_sum, 2));

            }
        }
        MPI_Send(&(modified[0][0]), rows * width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    }
    
    
    MPI_Finalize();

    return 0;
}