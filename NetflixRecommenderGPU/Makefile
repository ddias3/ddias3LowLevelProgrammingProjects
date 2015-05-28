naive:
	gcc -g -std=c99 -fopenmp netflix_naive.c -o Netflix -lm

nvcc:
	nvcc -arch=sm_20 netflix.cu -o Netflix -Xcompiler -fopenmp -lgomp

nvcc_ptxas:
	nvcc -arch=sm_20 netflix.cu -o Netflix -Xcompiler -fopenmp -lgomp --ptxas-options=-v

counter:
	gcc learning_data_counter.c -o Counter
	./Counter

clear:
	rm Counter
	rm Netflix
