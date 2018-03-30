#include<mpi.h> 
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define SOBEL 0
#define MEAN_REMOVAL 1


int* computeArrayfromMatrix(int start, int stop, int width, int** original) {
	int i, j, k;
	k =  0;

	int* outputArray = (int *) malloc(sizeof(int *) * width * (stop - start));

	// incerc sa iau elemente de la un anumit indice, dar in acelasi timp, sa le pun in ordine in vector
	for(i = start; i < stop; i ++) {
		for(j = 0; j < width; j ++) {
			outputArray[k] = original[i][j];
			k++;
		}

	}
	return outputArray;
}

int* computeMiniArrays(int start, int stop, int width, int* original) {
	int i, n = 0;

	int* outputArray = (int *) malloc(sizeof(int *) * width * (stop - start));

	// cam aceeasi idee ca mai sus, doar ca de data asta, iau elemente dintr-un vector
	for(i = start * width; i < stop * width; i++) {
		outputArray[n] = original[i];
		n++;
	}

	return outputArray;
}
	// a = originalArray;
int* applyFilter(int tag, int height, int width, int* a) {

	int *postFilterArray = (int *) malloc(sizeof(int *) * (width - 2) * (height - 2));
	int i, j = 0, it;
	if(tag == 0) {
		// sobel

		for(i = width + 1; i < (height - 1) * width - 1; i++) {
			postFilterArray[j] = a[i - width - 1] + 2 * a[i - 1] + a[i + width - 1] - a[i - width + 1] - 2 * a[i + 1] - a[i + width + 1] + 127;

			if(i % (width - 1) == 0) {
				i += 1;
				continue;
			}

			if(postFilterArray[j] < 0) {
				postFilterArray[j] = 0;
			} else if(postFilterArray[j] > 255) {
				postFilterArray[j] = 255;
			}
			j++;
		}

	} else {
		// celalalt

		for(i = width + 1; i < (height - 1) * width - 1; i++) {
			postFilterArray[j] = 9 * a[i] - a[i - width - 1] - a[i - width] - a[i - width + 1] - a[i - 1] - a[i + 1] - a[i + width - 1] - a[i + width] - a[i + width + 1];

			if(i % (width - 1) == 0) {
				i += 1;
				continue;
			}

			if(postFilterArray[j] < 0) {
				postFilterArray[j] = 0;
			} else if(postFilterArray[j] > 255) {
				postFilterArray[j] = 255;
			}
			j++;
		}
	}

	return postFilterArray;

}

int main(int argc, char * argv[]) {
	int rank;
	int nProcesses;
	int nCommands, i, j, k, n, tag;
	char* filter;
	char* imageIN;
	char* imageOUT;
	char str[50], firstLine[5], secondLine[50];
	char *token;
	int neighbours[20], noNeigh = 0;
	int *miniArray, *newMiniArray;
	int nextWidth, nextHeight, width, height;
	int **filteringMatrix;
	int statistica[nProcesses];

	MPI_Status status;

	MPI_Init(&argc, &argv);

	FILE *commandsFile, *topologyFile, *imageFile;

	filter = (char*) malloc (12 * sizeof(char));
	imageIN = (char*) malloc (30 * sizeof(char));
	imageOUT = (char*) malloc (30 * sizeof(char));

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	for( i = 0; i < nProcesses; i ++) {
		statistica[i] = 0;
	}

	// citire TOPOLOGIE.IN --------------------------------------------------------
	topologyFile = fopen(argv[1], "r");

	while(fgets (str, 60, topologyFile)){
		token = strtok(str, ":");

		if(atoi(token) == rank) {
			token = strtok(NULL, " ");

			while(token) {
				neighbours[noNeigh] = atoi(token);
				noNeigh++;

				token = strtok(NULL, " ");
			}
			break;
		}
	  }

	fclose(topologyFile);

	// citire IMIAGINI.IN -----------------------------------------------------------
	commandsFile = fopen(argv[2], "r");

	fscanf(commandsFile, "%d", &nCommands);

	for(k = 0; k < nCommands; k++) {
			
		fscanf(commandsFile, "%s", filter);
		fscanf(commandsFile, "%s", imageIN);
		fscanf(commandsFile, "%s", imageOUT);

		if(filter[0] == 's')
			tag = SOBEL;
		else
			tag = MEAN_REMOVAL;

		if(rank == 0) {

			imageFile = fopen(imageIN, "r");

			fgets (firstLine, 5, imageFile); 	//P2
			fgets (secondLine, 50, imageFile);	// ceva semnatura

			int pixel;

			fscanf(imageFile, "%d", &width);
			fscanf(imageFile, "%d", &height);

			fscanf(imageFile, "%s", str);		// 255

			width += 2;
			height += 2;

			int **imagePixels = (int **) malloc(sizeof(int *) * height);

			for (i = 0; i < height; i++) {
				imagePixels[i] = (int*) malloc(sizeof(int) * width);
			}

			for (i = 0; i < height; i++) {
				imagePixels[i][0] = 0;
				imagePixels[i][width - 1] = 0;
			}

			for (i = 0; i < width; i++) {
				imagePixels[0][i] = 0;
				imagePixels[height - 1][i] = 0;
			}

			for(i = 1; i < height - 1; i++) {

				for (j = 1; j < width - 1; j++) {
					fscanf(imageFile, "%d", &pixel);
					imagePixels[i][j] = pixel;
				}
			}

			fclose(imageFile);

			int ratio = (height - 2)/noNeigh; // am scazut 2 pentru ca 2 linii sunt in plus
			int startLine = 0, stopLine = 0, stopCounter = noNeigh;

			if(height < noNeigh){
				ratio = 1;
				stopCounter = height;
			}

			// compute mini_matrix

			for(i = 0; i < stopCounter; i++) {

				// in functie de ordinea copilului, acesta primeste mai putine sau mai multe linii
				if(i != noNeigh - 1) { 
					stopLine += ratio + 2;

				} else {
					stopLine = stopLine + ratio + (height - 2) % noNeigh;
				}

				miniArray = computeArrayfromMatrix(startLine, stopLine, width, imagePixels);

				nextWidth = width;
				nextHeight = stopLine - startLine;

				startLine += ratio;
				// trimit noile dimensiuni (width e aceeasi)
				MPI_Send(&nextWidth, 1, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);
				MPI_Send(&nextHeight, 1, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);

				MPI_Send(miniArray, nextWidth * nextHeight, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);

			 }

			int *finalArray = (int *) malloc(sizeof(int *) * (width - 2) * (height - 2)); // Elvis man...
			int senderStart = 0, senderStop = 0, it, recvHeight, finalStatistica[nProcesses];

			for (i = 0; i < stopCounter; i++) {
				MPI_Recv(&finalStatistica, nProcesses, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				MPI_Recv(&recvHeight, 1, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				miniArray = (int *) malloc(sizeof(int *) * (width - 2) * recvHeight);
				MPI_Recv(miniArray, (width - 2) * recvHeight, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

				senderStop += (width - 2) * recvHeight;
				int j = 0;

				for (it = senderStart; it < senderStop; it++) {
					finalArray[it] = miniArray[j];
					j++;
				}

				senderStart = senderStop;

				for (it = 0; it < nProcesses; ++it) {
					statistica[it] += finalStatistica[it];
					statistica[rank] = 0;
				}
			}
			// comute image
			FILE *fout = fopen(imageOUT, "w");
			fprintf(fout, "%s", firstLine);
			fprintf(fout, "%s", secondLine);
			fprintf(fout, "%d %d\n", (width - 2), (height - 2));
			fprintf(fout, "%d\n", 255);
			for (i = 0; i < (width - 2) * (height - 2); i++) {
				fprintf(fout, "%d\n", finalArray[i]);
			}

			fclose(fout);

		} else {

			MPI_Recv(&width, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			MPI_Recv(&height, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			miniArray = (int *) malloc(sizeof(int *) * width * height);

			MPI_Recv(miniArray, width * height, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			int parent = status.MPI_SOURCE;

			if(noNeigh == 1) {
				// FRUNZA

				int *postFilterArray = (int *) malloc(sizeof(int *) * (width - 2) * (height - 2));

				postFilterArray = applyFilter(status.MPI_TAG, height, width, miniArray);

				nextHeight = height - 2;

				statistica[rank] += nextHeight;

				MPI_Send(&statistica, nProcesses, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
				
				MPI_Send(&nextHeight, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
				MPI_Send(postFilterArray, (width - 2) * (height - 2), MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
				
			} else {

				// NOD INTERMEDIAR
				int ratio = (height - 2)/(noNeigh - 1);
				int startLine = 0, stopLine = 0, stopCounter = noNeigh, once = 1;

				if(height < noNeigh - 1) {
					ratio = 1;
					stopCounter = height;
				}

				for(i = 0; i < stopCounter; i++) {

					if( neighbours[i] == status.MPI_SOURCE) {
						continue;
					}

					if(i == 1) {
						stopLine = 2;
					}

					if(i != noNeigh - 1) {
						stopLine += ratio;

					} else {
						stopLine += ratio + (height - 2) % (noNeigh - 1);
					}

					newMiniArray = computeMiniArrays(startLine, stopLine, width, miniArray);

					// width nu se  modificakk
					nextWidth = width;
					nextHeight = stopLine - startLine;

					startLine += ratio;
					MPI_Send(&nextWidth, 1, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);
					MPI_Send(&nextHeight, 1, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);

					MPI_Send(newMiniArray, nextWidth * nextHeight, MPI_INT, neighbours[i], tag, MPI_COMM_WORLD);
				}


				int *returnToSender = (int *) malloc(sizeof(int *) * (width - 2) * (height - 2)); // Elvis man...
				int senderStart = 0, senderStop = 0, totalHeight = 0, it, recvHeight, interStatistica[nProcesses];

				for (i = 0; i < stopCounter; i++) {

					if( neighbours[i] == parent ) {
						continue;
					}
					MPI_Recv(&interStatistica, nProcesses, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

					MPI_Recv(&recvHeight, 1, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				 	int *interMiniArray = (int *) malloc(sizeof(int *) * (width - 2) * recvHeight);
					MPI_Recv(interMiniArray, (width - 2) * recvHeight, MPI_INT, neighbours[i], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

					for (it = 0; it < nProcesses; it++) {
						statistica[it] += interStatistica[it];
						statistica[rank] = 0;
					}
		
					senderStop += (width - 2) * recvHeight;

					int j = 0;
					for (it = senderStart; it < senderStop; it++) {

						returnToSender[it] = interMiniArray[j];
						j++;
					}

					senderStart = senderStop;
					totalHeight += recvHeight;
				}

				MPI_Send(&statistica, nProcesses, MPI_INT, parent, tag, MPI_COMM_WORLD);

				MPI_Send(&totalHeight, 1, MPI_INT, parent, tag, MPI_COMM_WORLD);
				MPI_Send(returnToSender, (width - 2) * totalHeight, MPI_INT, parent, tag, MPI_COMM_WORLD);
			}

		}

	}
	// statistica
	if(rank == 0){
		FILE *fstat = fopen(argv[3], "w");
		for (i = 0; i < nProcesses; i++) {
			fprintf(fstat ,"%d: %d\n",i, statistica[i]);
		}
		fclose(fstat);

		fclose(commandsFile);
	}

	MPI_Finalize();
	return 0;
}