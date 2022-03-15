#include <stdio.h>
#include <dpu.h>
#include <assert.h>
#include <stdint.h>
#include "mm.h"
#include <stdlib.h>
#include <time.h>

#ifndef DPU_BINARY
#define DPU_BINARY "mm_kernel"
#endif


T* matrix_multiplication1(T* A, T* B, uint32_t A_row, uint32_t A_col, uint32_t B_row, uint32_t B_col) {

    T* C = (T*)malloc(A_row * B_col * sizeof(T));
    for (int i = 0; i < A_row; i++) {
        for (int j = 0; j < B_col; j++) {
            C[B_col * i + j] = 0;  //C1[i][j]
            for (int k = 0; k < A_col;k++) {
                C[B_col * i + j] += A[A_col * i + k] * B[B_col * k + j];
            }
        }
    }

    return C;
}

T* matrix_multiplication2(T* A, T* B, uint32_t A_row, uint32_t A_col, uint32_t B_row, uint32_t B_col) {
    T temp;
    T* C = (T*)malloc(A_row * B_col * sizeof(T));
    for (int i = 0; i < A_row; i++) {
        for (int k = 0; k < A_col;k++) {
            temp = A[i * A_col + k];
            for (int j = 0; j < B_col; j++) {
                C[B_col * i + j] += temp * B[A_row * k + j];
            }
        }
    }

    return C;
}

void print_matrix(T* A, uint32_t A_row, uint32_t A_col) {
    for (int i = 0; i < A_row; i++) {
        for (int j = 0; j < A_col; j++) {
            printf("%5d", A[A_col * i + j]);
        }
        printf("\n");
    }
}

int main() {
    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    clock_t start1, start2, end1, end2;

    uint32_t A_row = 6, A_col = 6, B_row = 6, B_col = 6;
    uint32_t i;
    T* bufferA = (T*)malloc(A_row * A_col * sizeof(T));     //A[A_row][A_col]
    T* bufferB = (T*)malloc(B_row * B_col * sizeof(T));     //B[B_row][B_col]
    T* bufferD = (T*)malloc(A_row * B_col * sizeof(T));    //C2[A_row][B_col]
    //initialize
    for (int i = 0; i < A_row * A_col; i++) {
        bufferA[i] = i + 1;
    }
    for (int i = 0; i < B_row * B_col; i++) {
        bufferB[i] = i + 1;
    }

    //multiplication
    start1 = clock();
    T* bufferC1 = matrix_multiplication1(bufferA, bufferB, A_row, A_col, B_row, B_col);
    end1 = clock();
    start2 = clock();
    T* bufferC2 = matrix_multiplication2(bufferA, bufferB, A_row, A_col, B_row, B_col);
    end2 = clock();


    //print matrix
    printf("print matrix A which is %d x %d\n", A_row, A_col);
    print_matrix(bufferA, A_row, A_col);
    printf("print matrix B which is %d x %d\n", B_row, B_col);
    print_matrix(bufferB, B_row, B_col);
    printf("print matrix C1 which is %d x %d\n", A_row, B_col);
    print_matrix(bufferC1, A_row, B_col);
    printf("print matrix C2 which is %d x %d\n", A_row, B_col);
    print_matrix(bufferC2, A_row, B_col);

    printf("C1 time : %f\n", (end1 - start1));
    printf("C2 time : %f\n", (end2 - start2));

    nr_of_dpus = A_row * B_col;

    dpu_arguments_t input_arguments[nr_of_dpus];

    for (int i = 0; i < nr_of_dpus; i++) {
        input_arguments[i].size = (A_col + B_row * B_col) * sizeof(T);
        input_arguments[i].row = B_row;
        input_arguments[i].col = B_col;
    }

    uint32_t input_size_A = A_col * sizeof(T);
    uint32_t input_size_B = B_row * B_col * sizeof(T);
    uint32_t output_size_D = B_col * sizeof(T);

    DPU_ASSERT(dpu_alloc(nr_of_dpus, NULL, &dpu_set));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));

    i = 0;


    DPU_FOREACH(dpu_set, dpu, i) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, &input_arguments[i]));
    }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(input_arguments[0]), DPU_XFER_DEFAULT));
    //i=0; do not need this?
    i = 0;
    DPU_FOREACH(dpu_set, dpu, i) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, bufferA + input_size_A * i / sizeof(T)));
    }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_A, DPU_XFER_DEFAULT));

    i = 0;
    DPU_FOREACH(dpu_set, dpu, i) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, bufferB + input_size_B * i / sizeof(T)));
    }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, input_size_A, input_size_B, DPU_XFER_DEFAULT));

    DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
    i = 0;
    DPU_FOREACH(dpu_set, dpu, i) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, bufferD + input_size_A * i / sizeof(T)));
    }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, input_size_A, DPU_XFER_DEFAULT));

    printf("print matrix D which is %d x %d\n", A_row, B_col);
    print_matrix(bufferD, A_row, B_col);

}