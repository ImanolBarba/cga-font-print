/***************************************************************************
 *   main.c  --  This file is part of cga-font-print.                      *
 *                                                                         *
 *   Copyright (C) 2023 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   cga-font-print is free software: you can redistribute it and/or       *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   cga-font-print is distributed in the hope that it will be useful,     *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/


#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <png.h>

#define IMAGE_WIDTH 128
#define CHAR_SIZE 8
#define CHARS_PER_IMAGE_ROW 16
// RBGA 8-bit depth
#define IMAGE_PIXEL_SIZE 4 

size_t get_height(size_t file_size) {
  return CHAR_SIZE * (file_size / IMAGE_WIDTH);
}

int write_image(const char *output_name, png_bytepp rows, size_t width, size_t height) {
  FILE *fp = fopen(output_name, "wb");
  if(!fp) {
    printf("Unable to open output file\n");
    return 1;
  }

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) {
    printf("Unable to create PNG object\n");
    fclose(fp);
    return 1;
  }

  png_infop info = png_create_info_struct(png);
  if(!info) {
    printf("Unable to create PNG structures\n");
    fclose(fp);
    return 1;
  }

  if(setjmp(png_jmpbuf(png))) {
    printf("Error writing PNG file\n");
    fclose(fp);
    return 1;
  }

  png_init_io(png, fp);

  png_set_IHDR(
    png,
    info,
    width, height,
    8,
    PNG_COLOR_TYPE_RGBA,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png, info);
  png_write_image(png, rows);
  png_write_end(png, NULL);

  fclose(fp);

  png_destroy_write_struct(&png, &info);
  return 0;
}

size_t read_char_rom(int fd, png_bytepp row_data) {
  uint8_t current_char[CHAR_SIZE];
  size_t chars_processed = 0;
  ssize_t bytes_read;
  while((bytes_read = read(fd, current_char, CHAR_SIZE)) != 0) {
    if(bytes_read != CHAR_SIZE) {
      printf("Warning: %zd bytes at the end of file were not processed\n", bytes_read);
      break;
    }
    for(unsigned int row = 0; row < CHAR_SIZE; ++row) {
      uint8_t col_data = current_char[row];
      for(unsigned int col = 0; col < CHAR_SIZE; ++col) {
        // Bright magenta from CGA color palette
        uint8_t color[IMAGE_PIXEL_SIZE] = {0xFF, 0x55, 0xFF, 0xFF};
        if((chars_processed + (chars_processed / CHARS_PER_IMAGE_ROW)) % 2) {
          // Regular magenta from CGA color palette
          color[0] = 0xAA;
          color[1] = 0x00;
          color[2] = 0xAA;
        }
        // lil' endian
        if(col_data & 0x80) {
          memset(color, 0xFF, 3);
        }
        size_t out_row = (chars_processed / CHARS_PER_IMAGE_ROW) * CHAR_SIZE + row;
        size_t out_col = ((chars_processed % CHARS_PER_IMAGE_ROW) * CHAR_SIZE * IMAGE_PIXEL_SIZE) + col * IMAGE_PIXEL_SIZE;
        memcpy(&(row_data[out_row][out_col]), color, IMAGE_PIXEL_SIZE);
        col_data <<= 1;
      }
    }
    ++chars_processed;
  }
  
  return chars_processed;
}

int main(int argc, char *argv[]) {
  if(argc != 3 && argc != 4) {
    printf("Wrong argument count\n");
    printf("Usage: %s INPUT OUTPUT [OFFSET]\n", argv[0]);
    return 1;
  }

  size_t offset = 0;
  if(argc == 4) {
    offset = atoll(argv[3]);
  }

  // Open file
  printf("Reading character ROM from: %s\n", argv[1]);
  int fd = open(argv[1], O_RDONLY);
  if(fd == -1) {
    printf("Unable to open file: %s\n", strerror(errno));
    return 1;
  }

  // Get file size
  off_t file_size = lseek(fd, 0, SEEK_END);
  if(file_size == -1) {
    printf("Error seeking file: %s\n", strerror(errno));
    return 1;
  }
  file_size -= offset;
  if(lseek(fd, offset, SEEK_SET) != offset) { 
    printf("Error seeking file: %s\n", strerror(errno));
    return 1;
  }

  // Alloc memory
  size_t height = get_height(file_size);
  png_bytepp rows = malloc(height * sizeof(png_bytep));
  for(unsigned int i = 0; i < height; ++i) {
    rows[i] = calloc(sizeof(png_byte), IMAGE_PIXEL_SIZE * IMAGE_WIDTH);
  }

  // Read ROM and write characters to PNG bitmap
  size_t chars_processed = read_char_rom(fd, rows);
  size_t chars_expected = ((height / 8) * (IMAGE_WIDTH / 8));
  if(chars_processed != chars_expected) {
    printf("Error: Read %zu characters, but expected %zu \n", chars_processed, chars_expected);
    return 1;
  }
  close(fd);

  // Write output PNG image
  printf("Writing output image to: %s\n", argv[2]);
  if(write_image(argv[2], rows, IMAGE_WIDTH, height)) {
    printf("Error writing file\n");
  }

  // Free memory
  for(unsigned int i = 0; i < height; ++i) {
    free(rows[i]);
  }
  free(rows);

  return 0;
}