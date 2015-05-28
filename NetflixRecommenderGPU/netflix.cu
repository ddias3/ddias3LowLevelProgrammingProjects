#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>

const int FACTORS = 30;

int numberMovies = 0;
int numberUsers = 0;

#define LEARNING_RATE (0.00125)
#define REGULARIZATION_PARAMETER (0.005)

#define BLOCKSIZE (32)
#define LEARNING_DATA_SIZE (99072112)
#define ERROR_CHANGE_THRESHOLD (100)
#define ERROR_THRESHOLD (0.01)//(0.145)
#define WARP_SIZE (32)

#define gpuErrorCheck(x) { gpuAssert((x), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, char* fileName, int lineNumber, int abort = 1)
{
	if (code != cudaSuccess)
	{
		fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), fileName, lineNumber);
		if (abort)
			exit(code);
	}
}

typedef struct rating
{
	int rating;
	int userIndex;
	int movieIndex;
} rating_t;

rating_t* h_learningData;

double* h_userFactors;
double* h_movieFactors;

double get_walltime()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return (double)(tp.tv_sec + tp.tv_usec*1e-6);
}

void CreateLearningDataAndFactors()
{
	h_learningData = (rating_t*)malloc(sizeof(rating_t) * LEARNING_DATA_SIZE);
	assert(NULL != h_learningData);
	
	int indexLearningData = 0;
	FILE* netflixDataFile = fopen("netflix.dat", "r");
	char buffer[60];
	while (indexLearningData < LEARNING_DATA_SIZE && fgets(buffer, 60, netflixDataFile) != NULL)
	{
		assert(indexLearningData < LEARNING_DATA_SIZE);

		char* startPointer = buffer;
		char* endPointer = buffer;

		h_learningData[indexLearningData].userIndex = ((int)strtol(startPointer, &endPointer, 10)) - 1;
		startPointer = endPointer;
		h_learningData[indexLearningData].movieIndex = ((int)strtol(startPointer, &endPointer, 10)) - 1;
		startPointer = endPointer;
		h_learningData[indexLearningData].rating = ((int)strtol(startPointer, &endPointer, 10));

		if (h_learningData[indexLearningData].userIndex > numberUsers)
			numberUsers = h_learningData[indexLearningData].userIndex;
		if (h_learningData[indexLearningData].movieIndex > numberMovies)
			numberMovies = h_learningData[indexLearningData].movieIndex;
		
		++indexLearningData;
	}
	++numberMovies;
	++numberUsers;

	printf("Allocating memory for user and movie factors\n");

	h_userFactors = (double*)malloc(sizeof(double) * FACTORS * numberUsers);
	assert(NULL != h_userFactors);
	h_movieFactors = (double*)malloc(sizeof(double) * FACTORS * numberMovies);
	assert(NULL != h_movieFactors);

	#pragma omp parallel for
	for (int n = 0; n < (FACTORS * numberUsers); ++n)
		h_userFactors[n] = 1.0;

	int index = 0;
	int currentMovieIndex = 0;
	size_t sumRating = 0;
	size_t timesAdded = 0;
	while (index < LEARNING_DATA_SIZE)
	{
		if (h_learningData[index].movieIndex != currentMovieIndex)
		{
			for (int f = 0; f < FACTORS; ++f)
				h_movieFactors[currentMovieIndex * FACTORS + f] = (sumRating / ((double)timesAdded)) / ((double)FACTORS);

			currentMovieIndex = h_learningData[index].movieIndex;
			sumRating = 0;
			timesAdded = 0;
			continue;
		}

		sumRating += h_learningData[index].rating;
		++timesAdded;
		++index;
	}
	for (int f = 0; f < FACTORS; ++f)
		h_movieFactors[currentMovieIndex * FACTORS + f] = (sumRating / ((double)timesAdded)) / ((double)FACTORS);
	
	printf("Randomizing the learning data\n");

	srand(time(NULL));

	for (int n = 0; n < (LEARNING_DATA_SIZE / numberMovies); ++n)
	{
		rating_t tempRatingBlock;
		size_t m = ((rand() % numberMovies) * (LEARNING_DATA_SIZE / numberMovies)) + (rand() % (LEARNING_DATA_SIZE / numberMovies));
		assert(m < LEARNING_DATA_SIZE);

		tempRatingBlock.rating = h_learningData[n].rating;
		tempRatingBlock.userIndex = h_learningData[n].userIndex;
		tempRatingBlock.movieIndex = h_learningData[n].movieIndex;

		h_learningData[n].rating = h_learningData[m].rating;
		h_learningData[n].userIndex = h_learningData[m].userIndex;
		h_learningData[n].movieIndex = h_learningData[m].movieIndex;

		h_learningData[m].rating = tempRatingBlock.rating;
		h_learningData[m].userIndex = tempRatingBlock.userIndex;
		h_learningData[m].movieIndex = tempRatingBlock.movieIndex;
	}
}

__host__ __device__ double DotProduct(const double* vectorA, const double* vectorB, const int length)
{
	double dotProduct = 0.0;

	for (int n = 0; n < length; ++n)
		dotProduct += vectorA[n] * vectorB[n];

	return dotProduct;
}

__global__ void SingleIteration(const int learningDataLength, double* sumErrorSquared, rating_t* learningData, double* userFactors, double* movieFactors)
{
	//int index_x = blockIdx.x * blockDim.x + threadIdx.x;
	//int index_y = blockIdx.y * blockDim.y + threadIdx.y;
	//int index = index_y * gridDim.x * blockDim.x + index_x;
	int indexKnownData = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
	if (indexKnownData == 0)
		*sumErrorSquared = 0.0;
	if (indexKnownData < learningDataLength)
	{
		int userIndex = learningData[indexKnownData].userIndex;
		int movieIndex = learningData[indexKnownData].movieIndex;
		int rating = learningData[indexKnownData].rating;

		double errorTerm = rating - DotProduct(&movieFactors[movieIndex * FACTORS], &userFactors[userIndex * FACTORS], FACTORS);
		//*sumErrorSquared += (errorTerm * errorTerm);

		for (int f = 0; f < FACTORS; ++f)
		{
			movieFactors[movieIndex * FACTORS + f] += LEARNING_RATE * (errorTerm * userFactors[userIndex * FACTORS + f]
				- REGULARIZATION_PARAMETER * movieFactors[movieIndex * FACTORS + f]);
		}

		for (int f = 0; f < FACTORS; ++f)
		{
			userFactors[userIndex * FACTORS + f] += LEARNING_RATE * (errorTerm * movieFactors[movieIndex * FACTORS + f]
				- REGULARIZATION_PARAMETER * userFactors[userIndex * FACTORS + f]);
		}
	}
}

__global__ void CalculateAggregateError(const int learningDataLength, double* sumErrorSquared, rating_t* learningData, double* userFactors, double* movieFactors)
{
	int indexKnownData = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
	if (indexKnownData < learningDataLength)
	{
		int userIndex = learningData[indexKnownData].userIndex;
		int movieIndex = learningData[indexKnownData].movieIndex;
		int rating = learningData[indexKnownData].rating;

		double errorTerm = rating - DotProduct(&movieFactors[movieIndex * FACTORS], &userFactors[userIndex * FACTORS], FACTORS);
		*sumErrorSquared += (errorTerm * errorTerm);
	}
}

void PrintPredictedData()
{
	int indexQueryData = 0;
	FILE* netflixQueryFile = fopen("netflix_query", "r");
	char buffer[60];
	while (fgets(buffer, 60, netflixQueryFile) != NULL)
	{
		char* startPointer = buffer;
		char* endPointer = buffer;

		int userIndex = ((int)strtol(startPointer, &endPointer, 10));
		startPointer = endPointer;
		int movieIndex = ((int)strtol(startPointer, &endPointer, 10));
		startPointer = endPointer;

		assert(userIndex < numberUsers);
		assert(movieIndex < numberMovies);

		double predictedRating = DotProduct(&h_movieFactors[movieIndex * FACTORS], &h_userFactors[userIndex * FACTORS], FACTORS);

		printf("User#:%d\tMovie#:%d\tPredicted Rating = %lf\n", userIndex, movieIndex, predictedRating);

		++indexQueryData;
	}
}

int main(int argc, char** argv)
{
	printf("Starting Netflix Recommender System Algorithm with Stochastic Gradient Descent\n");
	printf("Creating Learning Data\n");

	CreateLearningDataAndFactors();

	printf("Copying Data to GPU\n");

	double* h_sumErrorSquared = (double*)malloc(sizeof(double));
	double* d_sumErrorSquared;
	*h_sumErrorSquared = 0.0;
	gpuErrorCheck(cudaMalloc((void**)&d_sumErrorSquared, sizeof(double)));
	gpuErrorCheck(cudaMemcpy(d_sumErrorSquared, h_sumErrorSquared, sizeof(double), cudaMemcpyHostToDevice));

	double* d_userFactors;
	double* d_movieFactors;
	rating_t* d_learningData;

	gpuErrorCheck(cudaMalloc((void**)&d_userFactors, sizeof(double) * FACTORS * numberUsers));
	gpuErrorCheck(cudaMalloc((void**)&d_movieFactors, sizeof(double) * FACTORS * numberMovies));
	gpuErrorCheck(cudaMalloc((void**)&d_learningData, sizeof(rating_t) * LEARNING_DATA_SIZE));

	gpuErrorCheck(cudaMemcpy(d_userFactors, h_userFactors, sizeof(double) * FACTORS * numberUsers, cudaMemcpyHostToDevice));
	gpuErrorCheck(cudaMemcpy(d_movieFactors, h_movieFactors, sizeof(double) * FACTORS * numberMovies, cudaMemcpyHostToDevice));
	gpuErrorCheck(cudaMemcpy(d_learningData, h_learningData, sizeof(rating_t) * LEARNING_DATA_SIZE, cudaMemcpyHostToDevice));

	printf("Starting Algorith now\n");
	fflush(stdout);

	dim3 blockSize;
	blockSize.x = BLOCKSIZE;

	dim3 gridSize;

	int numberBlocksRequired = ((LEARNING_DATA_SIZE - 1) / BLOCKSIZE) + 1;

	if (numberBlocksRequired > 65535)
	{
		gridSize.x = 65535;
		gridSize.y = (numberBlocksRequired - 1) / 65535 + 1;
	}
	else
	{
		gridSize.x = numberBlocksRequired;
		gridSize.y = 1;
	}

	double previousErrorSquared = 9999999;
	double previousPreviousErrorSquared;

	unsigned int iterations = 0;
	double startTime = get_walltime();
	do
	{
		previousPreviousErrorSquared = previousErrorSquared;
		previousErrorSquared = *h_sumErrorSquared;

		SingleIteration<<<gridSize, blockSize>>>(LEARNING_DATA_SIZE, d_sumErrorSquared, d_learningData, d_userFactors, d_movieFactors);
		gpuErrorCheck(cudaPeekAtLastError());
		gpuErrorCheck(cudaDeviceSynchronize());

		CalculateAggregateError<<<gridSize, blockSize>>>(LEARNING_DATA_SIZE, d_sumErrorSquared, d_learningData, d_userFactors, d_movieFactors);
		gpuErrorCheck(cudaPeekAtLastError());
		gpuErrorCheck(cudaDeviceSynchronize());

		gpuErrorCheck(cudaMemcpy(h_sumErrorSquared, d_sumErrorSquared, sizeof(double), cudaMemcpyDeviceToHost));

		printf("\th_sumErrorSquared = %lf, (1/N(h_sumErrorSquared * WARP_SIZE))^(1/2) = %lf\n", *h_sumErrorSquared, sqrt(1.0 / LEARNING_DATA_SIZE * (*h_sumErrorSquared * WARP_SIZE)));
		++iterations;

		if (*h_sumErrorSquared > previousErrorSquared && previousErrorSquared > previousPreviousErrorSquared ||
			(abs(previousErrorSquared - *h_sumErrorSquared) + abs(previousPreviousErrorSquared - previousErrorSquared)) / 2.0 < ERROR_CHANGE_THRESHOLD)
			break;
	} while (*h_sumErrorSquared > (double)LEARNING_DATA_SIZE / (double)WARP_SIZE * (ERROR_THRESHOLD * ERROR_THRESHOLD));
	double endTime = get_walltime();
	
	printf("Performed %u iterations\n", iterations);

	printf("Total time = %lf\n", endTime - startTime);
	printf("Copying user and movie factors from GPU to host\n");

	gpuErrorCheck(cudaMemcpy(h_userFactors, d_userFactors, sizeof(double) * FACTORS * numberUsers, cudaMemcpyDeviceToHost));
	gpuErrorCheck(cudaMemcpy(h_movieFactors, d_movieFactors, sizeof(double) * FACTORS * numberMovies, cudaMemcpyDeviceToHost));

	PrintPredictedData();

	gpuErrorCheck(cudaFree(d_sumErrorSquared));
	gpuErrorCheck(cudaFree(d_userFactors));
	gpuErrorCheck(cudaFree(d_movieFactors));
	gpuErrorCheck(cudaFree(d_learningData));

	free(h_sumErrorSquared);
	free(h_userFactors);
	free(h_movieFactors);
	free(h_learningData);

	return 0;
}
