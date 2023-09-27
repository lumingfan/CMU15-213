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
void transposeBy32(int A[32][32], int B[32][32]) {
    for (int i = 0; i < 32; i += 8) {
        for (int j = 0; j < 32; j += 8) {
            for (int k = i; k < i + 8; ++k) {
                int a0 = A[k][j];
                int a1 = A[k][j + 1];
                int a2 = A[k][j + 2];
                int a3 = A[k][j + 3];
                int a4 = A[k][j + 4];
                int a5 = A[k][j + 5];
                int a6 = A[k][j + 6];
                int a7 = A[k][j + 7];
            
                B[j][k] = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;
                B[j + 4][k] = a4;
                B[j + 5][k] = a5;
                B[j + 6][k] = a6;
                B[j + 7][k] = a7;
            }
        }
    }
}

void transposeBy6167(int A[67][61], int B[61][67]) {
    int bsize = 16;
    for (int i = 0; i < 67; i += bsize) {
        for (int j = 0; j < 67; j += bsize) {
            for (int k = i; k < 67 && k < i + bsize; ++k) {
                for (int l = j; l < 61 && l < j + bsize; ++l) {
                    B[l][k] = A[k][l];
                }
            }
        }
    }
}


void transposeBy64(int A[64][64], int B[64][64]) {
    for (int i = 0; i < 64; i += 8) {
        for (int j = 0; j < 64; j += 8) {
            for (int k = i; k < i + 4; ++k) {
                int a0 = A[k][j];
                int a1 = A[k][j + 1];
                int a2 = A[k][j + 2];
                int a3 = A[k][j + 3];
                int a4 = A[k][j + 4];
                int a5 = A[k][j + 5];
                int a6 = A[k][j + 6];
                int a7 = A[k][j + 7];

                B[j][k] = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;
                B[j][k + 4] = a4;
                B[j + 1][k + 4] = a5;
                B[j + 2][k + 4] = a6;
                B[j + 3][k + 4] = a7;
            }

            for (int l = j + 4; l < j + 8; ++l) {
                int a0 = A[i + 4][l - 4];
                int a1 = A[i + 5][l - 4];
                int a2 = A[i + 6][l - 4];
                int a3 = A[i + 7][l - 4];

                int a4 = B[l - 4][i + 4];
                int a5 = B[l - 4][i + 5];
                int a6 = B[l - 4][i + 6];
                int a7 = B[l - 4][i + 7];

                B[l - 4][i + 4] = a0; 
                B[l - 4][i + 5] = a1;
                B[l - 4][i + 6] = a2;
                B[l - 4][i + 7] = a3;

                B[l][i] = a4;
                B[l][i + 1] = a5;
                B[l][i + 2] = a6;
                B[l][i + 3] = a7;

                B[l][i + 4] = A[i + 4][l];
                B[l][i + 5] = A[i + 5][l];
                B[l][i + 6] = A[i + 6][l];
                B[l][i + 7] = A[i + 7][l];
            }

        }
    }
}


char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (M == 32 && N == 32) {
        transposeBy32(A, B);
    }
    if (M == 64 && N == 64) {
        transposeBy64(A, B);
    }
    if (M == 61 && N == 67) {
        transposeBy6167(A, B);
    }
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

