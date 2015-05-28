#include "my_malloc.h"
#include <stdio.h>

int main()
{
	/*
	 * Test code goes here
	 */
	
	printf("\n--------------------\nBegin Test Code\n\n");
	
	int* x = my_malloc(sizeof(int));
	*x = 123456;
	printf("x = %d\n\n", *x);
	
	int* y = my_malloc(sizeof(int));
	*y = 3245;
	printf("y = %d\n\n", *y);
	
	char* z = my_malloc(sizeof(char));
	*z = 123;
	printf("z = %d\n\n", *z);
	
	int* w = my_malloc(sizeof(int));
	*w = 1;
	printf("w = %d\n\n", *w);
	
	short* v = my_malloc(sizeof(short));
	*v = 999;
	printf("v = %d\n\n", *v);
	
	int* u = my_malloc(sizeof(int));
	*u = 707;
	printf("u = %d\n\n", *u);
	
	int* q = my_malloc(sizeof(int));
	*q = 909;
	printf("q = %d\n\n", *q);
	
	int* p = my_malloc(sizeof(int));
	*p = -987;
	printf("p = %d\n\n", *p);
	
	int* o = my_malloc(sizeof(int));
	*o = -123;
	printf("o = %d\n\n", *o);
	
	my_free(x);
	printf("just called my_free(x) first time\n\n");
	
	my_free(y);
	printf("just called my_free(y) first time\n\n");
	
	x = my_malloc(sizeof(int));
	*x = 654321;
	printf("x = %d\n\n", *x);
	
	y = my_malloc(sizeof(int));
	*y = 543;
	printf("y = %d\n\n", *y);
	
	my_free(y);
	printf("just called my_free(y) second time\n\n");
	
	my_free(x);
	printf("just called my_free(x) second time\n\n");
	
	printf("size of long = %d, with 10 elements, that requires %d bytes\n\n", (int)sizeof(long), (int)sizeof(long)*10);
	long* zz = my_calloc(10, sizeof(long));
	
	z[5] = 10;
	z[3] = 78;
	z[9] = 100;
	
	for (int n = 0; n < 10; ++n)
		printf("z[%d] = %d\n", n, (int)z[n]);
		
	// my_free(x);
	// my_free(y);
	my_free(zz);
	my_free(z);
	my_free(w);
	my_free(v);
	my_free(u);
	my_free(q);
	my_free(p);
	my_free(o);
		
	/*int* a = my_malloc(1000);
	
	int* b = my_malloc(1000);
	
	int* c = my_malloc(1000);
	
	int* d = my_malloc(1000);
	
	my_free(d);
	
	my_free(a);
	
	my_free(c);
	
	my_free(b);*/
	
	return 0;
}
