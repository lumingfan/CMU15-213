/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the Number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do Not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";

void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    int bsize = 32 * 8 / M;
    if (M == 61) 
        bsize = 12;
    int rowend = bsize * (N / bsize);
    int colend = bsize * (M / bsize);
    int i, j, k, l;
    // block
    for (i = 0; i < rowend; i += bsize) {

        for (j = 0; j < colend; j += bsize) { 
            for (int c = 0; c < bsize; ++c) {
                k = i + c;
                l = j + c;
                B[l][k] = A[k][l];
                for (k = i; k < i + bsize; ++k) {
                    if (k == i + c)
                        continue;
                    B[l][k] = A[k][l];
                }
            }
        }
    }


    if (M == N)
        return ;

    // a: 67 x 61
    for (k = i; k < N; ++k) 
        for (l = j; l < M; ++l)
            B[l][k] = A[k][l];
    
    for (k = i; k < N; ++k)
        for (l = 0; l < j; ++l)
            B[l][k] = A[k][l];

    for (k = j; k < M; ++k) 
        for (l = 0; l < i; ++l)
            B[k][l] = A[l][k];

}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, Not optimized for the cache.
 */
char *trans_zig_zag_desc = "Block zig zag scan transpose";
void transpose_zig_zag(int M, int N, int A[N][M], int B[M][N]) {
    int bsize = 8 * 32 / M;

    int rowEnd = bsize * (N / bsize); 
    int colEnd = bsize * (M / bsize);

    int i, j, k, l;
    // block
    for (i = 0; i < rowEnd; i += bsize) {
        for (j = 0; j < colEnd; j += bsize) {

            // zig-zag
            k = i;
            l = j + bsize - 1;
            int diag = 1;

            while (diag <= bsize) {
                for (int c = 0; c < diag; ++c) {
                    B[l][k] = A[k][l];
                    ++k;
                    ++l;
                }
                ++diag;
                k = i;
                l = j + bsize - diag;
            }

            diag -= 2;
            k = i + 1;
            l = j;
            while (diag > 0) {
                for (int c = 0; c < diag; ++c) {
                    B[l][k] = A[k][l];
                    ++k;
                    ++l; 
                }
                --diag;
                k = i + bsize - diag;
                l = j; 
            }
        }
    }

    // if (M == N)
      //  return ;


    // A: 67 x 61
    for (k = i; k < N; ++k) 
        for (l = j; l < M; ++l)
            B[l][k] = A[k][l];
    
    for (k = i; k < N; ++k)
        for (l = 0; l < j; ++l)
            B[l][k] = A[k][l];

    for (k = j; k < M; ++k) 
        for (l = 0; l < i; ++l)
            B[k][l] = A[l][k];
}


char *trans_block_desc = "Block reverse row-wise scan transpose";
void transpose_block(int M, int N, int A[N][M], int B[M][N]) {
    int bsize = 32 * 8 / M;
    if (M == 61) 
        bsize = 12;
    int rowEnd = bsize * (N / bsize);
    int colEnd = bsize * (M / bsize);
    int i, j, k, l;
    // block
    for (i = 0; i < rowEnd; i += bsize) {
        for (j = 0; j < colEnd; j += bsize) { 
            for (int c = 0; c < bsize; ++c) {
                k = i + c;
                l = j + c;
                B[l][k] = A[k][l];

                for (k = i; k < i + bsize; ++k) {
                    if (k == i + c) {
                        continue;
                    } 
                    B[l][k] = A[k][l];
                }
            }
        }
    }


    if (M == N)
        return ;

    // A: 67 x 61
    for (k = i; k < N; ++k) 
        for (l = j; l < M; ++l)
            B[l][k] = A[k][l];
    
    for (k = i; k < N; ++k)
        for (l = 0; l < j; ++l)
            B[l][k] = A[k][l];

    for (k = j; k < M; ++k) 
        for (l = 0; l < i; ++l)
            B[k][l] = A[l][k];
}


char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

char trans_col_wise[] = "Simple col-wise scan transpose";
void trans_col(int M, int N, int A[N][M], int B[M][N]) {
    for (int j = 0; j < M; ++j) {
        for (int i = 0; i < N; ++i) {
            B[j][i] = A[i][j];
        }
    }
}

char trans_reverse_wise[] = "Reverse row-wise scan transpose";
void trans_reverse(int M, int N, int A[N][M], int B[M][N]) {
    for (int i = 0; i < N; ++i) {
        for (int j = M - 1; j >= 0; --j)
            B[j][i] = A[i][j];
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 
    // registerTransFunction(transpose_block, trans_block_desc);
    // registerTransFunction(transpose_zig_zag, trans_zig_zag_desc);

    /* Register any additional transpose functions */
    // registerTransFunction(trans, trans_desc); 

    // registerTransFunction(trans_col, trans_col_wise);
    // registerTransFunction(trans_reverse, trans_reverse_wise);

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning froM the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

