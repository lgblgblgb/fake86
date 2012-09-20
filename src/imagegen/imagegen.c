/*
  Imagegen: A blank disk image generator for use with Fake86
  Copyright (C)2010-2012 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>

const char *build = "Imagegen v1.1";

int main(int argc, char *argv[]) {
	FILE			*image;
	char			*blank;
	unsigned long	i, size;

	printf("%s (c)2010-2012 Mike Chambers\n", build);
	printf("[Blank disk image generator for Fake86]\n\n");

	if(argc < 3) {
		printf("Usage syntax:\n");
		printf("    imagegen imagefile size\n\n");
		printf("imagefile denotes the filename of the new disk image to create.\n");
		printf("size denotes the size in megabytes that it should be.\n");
		return(1);
	}

	size = atoi(argv[2]);
	if((size > 503) || !size) {
		printf("Invalid size specified! Valid range is 1 to 503 MB.\n");
		return(1);
	}

	image = fopen(argv[1], "wb");
	if(image == NULL) {
		printf("Unable to create new file: %s\n", argv[1]);
		return(1);
	}

	blank = (void *)malloc(1048576);
	if (blank == NULL) {
		printf("Unable to allocate enough memory!\n");
		return(1);
	}

	printf("Please wait, generating new image...\n");

	for(i = 0; i < size; i++) {
		fwrite(&blank[0], 1048576, 1, image);
		printf("\rWriting to file: %u MB", i);
	}

	printf(" complete.\n");
	return(0);
}
