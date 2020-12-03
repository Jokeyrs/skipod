#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#define Max(a, b) ((a) > (b) ? (a) : (b))

#define N (2*2*2*2*2*2*2*2+2)
double maxeps = 0.1e-7;
int itmax = 100;
int i, j, k;
double w = 0.5;
double eps;
double b;

double A[N][N][N];

void relax();
void init();
void verify();

int nproc, rank, start_row, end_row, nrow;
MPI_Request requests[4];
MPI_Status statuses[4];

int main(int argc, char **argv)
{
	int it;

	MPI_Init(&argc, &argv); 
	MPI_Comm_size(MPI_COMM_WORLD, &nproc); 
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	start_row = (N - 2) / nproc * rank + 1; 
	end_row = (N - 2) / nproc * (rank + 1) + 1;
	nrow = end_row - start_row;

	double start = MPI_Wtime();

	init();

	for (it = 1; it <= itmax; it++)
	{
		eps = 0.;
		relax();
		// printf("it=%4i   eps=%f\n", it, eps);
		if (eps < maxeps)
			break;
	}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Gather(A[start_row], nrow * N * N, MPI_DOUBLE, A[1], nrow * N * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	double end = MPI_Wtime();
	if (!rank) {
		verify();
		printf("%lf\n", end - start);
	}

	MPI_Finalize();
	return 0;
}

void init()
{
	for (i = start_row; i < end_row; i++)
		for (j = 0; j <= N - 1; j++)
		for (k = 0; k <= N - 1; k++)
		{
			if (j == 0 || j == N - 1 || k == 0 || k == N - 1)
				A[i][j][k] = 0.;
			else
				A[i][j][k] = (4. + i + j + k);
		}
}

void share_end_row() {
	if (rank) {
		MPI_Irecv(A[start_row - 1], N * N, MPI_DOUBLE, rank - 1, 315, MPI_COMM_WORLD, requests);
	}
	if (rank != nproc - 1) {
		MPI_Isend(A[end_row - 1], N * N, MPI_DOUBLE, rank + 1, 315, MPI_COMM_WORLD, requests + 2);
	}
}

void share_start_row() {
	if (rank != nproc - 1) {
		MPI_Irecv(A[end_row], N * N, MPI_DOUBLE, rank + 1, 316, MPI_COMM_WORLD, requests + 3);
	}

	if (rank) {
		MPI_Isend(A[start_row], N * N, MPI_DOUBLE, rank - 1, 316, MPI_COMM_WORLD, requests + 1);
	}
}

void waitall() {
	
	int count = 4, shift = 0;

	if (!rank) {
		count -= 2;
		shift = 2;
	}
	if (rank == nproc - 1) {
		count -= 2;
	}
	
	MPI_Waitall(count, requests + shift, statuses);
}

void relax()
{
	double private_eps = 0.0;
	double local_eps = 0.0;

	share_end_row();
	share_start_row();
	waitall();

	for (i = start_row; i < end_row; i++)
	for (j = 1; j <= N - 2; j++)
		for (k = 1 + (i + j) % 2; k <= N - 2; k += 2)
		{
			b = w * ((A[i - 1][j][k] + A[i + 1][j][k] + A[i][j - 1][k] + A[i][j + 1][k] + A[i][j][k - 1] + A[i][j][k + 1]) / 6. - A[i][j][k]);
			private_eps = Max(fabs(b), private_eps);
			A[i][j][k] = A[i][j][k] + b;
		}

	share_end_row();
	share_start_row();
	waitall();

	for (i = start_row; i < end_row; i++)
	for (j = 1; j <= N - 2; j++)
		for (k = 1 + (i + j + 1) % 2; k <= N - 2; k += 2)
		{
			b = w * ((A[i - 1][j][k] + A[i + 1][j][k] + A[i][j - 1][k] + A[i][j + 1][k] + A[i][j][k - 1] + A[i][j][k + 1]) / 6. - A[i][j][k]);
			A[i][j][k] = A[i][j][k] + b;
		}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Reduce(&private_eps, &eps, 1 , MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	MPI_Bcast(&eps, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

void verify()
{
	double s;

	s = 0.;

	for (i = 0; i <= N - 1; i++)
	for (j = 0; j <= N - 1; j++)
	for (k = 0; k <= N - 1; k++)
	{
		s = s + A[i][j][k] * (i + 1) * (j + 1) * (k + 1) / (N * N * N);
	}
	// printf("  S = %f\n",s);
}
