#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    size_t clientes;
    size_t recursos;
    int *disponivel;
    int *maximo;
    int *alocado;
} Banco;


int calcular_necessidade(int maximo, int alocado)
{
    return maximo - alocado;
}
static void erro(const char *mensagem)
{
    fprintf(stderr, "Erro: %s\n", mensagem);
    exit(EXIT_FAILURE);
}