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

static FILE *abrir(const char *nome, const char *modo)
{
    FILE *arquivo = fopen(nome, modo);
    if (arquivo == NULL) {
        perror(nome);
        exit(EXIT_FAILURE);
    }
    return arquivo;
}

static void *alocar(size_t quantidade, size_t tamanho)
{
    if (quantidade > SIZE_MAX / tamanho)
        erro("quantidade de dados grande demais");

    void *memoria = calloc(quantidade, tamanho);
    if (memoria == NULL)
        erro("memória insuficiente");
    return memoria;
}

static char *pular_espacos(char *texto)
{
    while (isspace((unsigned char)*texto))
        texto++;
    return texto;
}

static int ler_inteiro(char **cursor)
{
    char *inicio = pular_espacos(*cursor);
    char *fim;
    errno = 0;
    long numero = strtol(inicio, &fim, 10);

    if (inicio == fim || errno == ERANGE || numero < 0 || numero > INT_MAX)
        erro("esperado um inteiro não negativo dentro do limite de int");

    *cursor = fim;
    return (int)numero;
}

static bool ler_linha(FILE *arquivo, char **linha, size_t *capacidade)
{
    ssize_t tamanho = getline(linha, capacidade, arquivo);
    if (tamanho < 0) {
        if (!feof(arquivo) || ferror(arquivo))
            erro("falha na leitura de um arquivo de entrada");
        return false;
    }
    if (memchr(*linha, '\0', (size_t)tamanho) != NULL)
        erro("arquivo de entrada contém um caractere inválido");
    return true;
}

static void carregar_clientes(Banco *banco, FILE *arquivo)
{
    char *linha = NULL;
    size_t capacidade = 0;

    while (ler_linha(arquivo, &linha, &capacidade))
        banco->clientes++;

    if (banco->clientes == 0)
        erro("customer.txt está vazio");
    if (banco->clientes > SIZE_MAX / banco->recursos)
        erro("matriz de clientes grande demais");

    size_t total = banco->clientes * banco->recursos;
    banco->maximo = alocar(total, sizeof *banco->maximo);
    banco->alocado = alocar(total, sizeof *banco->alocado);

    if (fseek(arquivo, 0, SEEK_SET) != 0)
        erro("não foi possível reler customer.txt");

    for (size_t cliente = 0; cliente < banco->clientes; cliente++) {
        if (!ler_linha(arquivo, &linha, &capacidade))
            erro("customer.txt mudou durante a leitura");

        char *p = linha;
        for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
            size_t pos = cliente * banco->recursos + recurso;
            banco->maximo[pos] = ler_inteiro(&p);
            p = pular_espacos(p);
            if (recurso + 1 < banco->recursos) {
                if (*p != ',')
                    erro("customer.txt deve separar os valores por vírgulas");
                p++;
            }
        }
        if (*p != '\0')
            erro("quantidade de valores incorreta em customer.txt");
    }
    if (ler_linha(arquivo, &linha, &capacidade))
        erro("customer.txt mudou durante a leitura");
    free(linha);
}

static bool estado_seguro(const Banco *banco)
{

    int *trabalho = alocar(banco->recursos, sizeof *trabalho);

    bool *terminou = alocar(banco->clientes, sizeof *terminou);

    memcpy(trabalho, banco->disponivel,
           banco->recursos * sizeof *trabalho);

    size_t concluidos = 0;

    bool avancou;

    do {

        avancou = false;

        for (size_t cliente = 0; cliente < banco->clientes; cliente++) {

            if (terminou[cliente])
                continue;

            size_t base = cliente * banco->recursos;

            size_t recurso = 0;

            while (recurso < banco->recursos &&
                   calcular_necessidade(banco->maximo[base + recurso],
                                        banco->alocado[base + recurso])
                       <= trabalho[recurso]) {

                recurso++;
            }

            if (recurso != banco->recursos)
                continue;

            for (recurso = 0; recurso < banco->recursos; recurso++) {

                trabalho[recurso] += banco->alocado[base + recurso];
            }

            terminou[cliente] = true;

            concluidos++;

            avancou = true;
        }

    } while (avancou && concluidos < banco->clientes);

    free(trabalho);
    free(terminou);

    return concluidos == banco->clientes;
}

static void escrever_vetor(FILE *saida, const int *valores, size_t quantidade)
{
    for (size_t i = 0; i < quantidade; i++)
        fprintf(saida, "%s%d", i == 0 ? "" : " ", valores[i]);
}

static void mostrar_estado_inicial(const Banco *banco, FILE *saida)
{
    fputs("MAXIMUM   |   ALLOCATION  |   NEED\n", saida);
    for (size_t cliente = 0; cliente < banco->clientes; cliente++) {
        size_t base = cliente * banco->recursos;
        escrever_vetor(saida, banco->maximo + base, banco->recursos);
        fputs("     |   ", saida);
        escrever_vetor(saida, banco->alocado + base, banco->recursos);
        fputs("       |   ", saida);
        for (size_t i = 0; i < banco->recursos; i++) {
            int falta = calcular_necessidade(banco->maximo[base + i],
                                             banco->alocado[base + i]);
            fprintf(saida, "%d ", falta);
        }
        fputc('\n', saida);
    }
    fputs("AVAILABLE ", saida);
    escrever_vetor(saida, banco->disponivel, banco->recursos);
    fputs(" \n", saida);
}

int main(int argc, char **argv)
{
    if (argc < 2)
        erro("uso: ./banker <quantidade do recurso 1> ...");

    Banco banco = {0};
    banco.recursos = (size_t)(argc - 1);
    banco.disponivel = alocar(banco.recursos, sizeof *banco.disponivel);
    for (size_t i = 0; i < banco.recursos; i++) {
        char *p = argv[i + 1];
        banco.disponivel[i] = ler_inteiro(&p);
        if (*pular_espacos(p) != '\0')
            erro("argumento de recurso inválido");
    }

    FILE *clientes = abrir("customer.txt", "r");
    carregar_clientes(&banco, clientes);
    fclose(clientes);

    mostrar_estado_inicial(&banco, stdout);

    if (estado_seguro(&banco)) {
        puts("Estado inicial seguro.");
    } else {
        puts("Estado inicial inseguro.");
    }

    free(banco.alocado);
    free(banco.maximo);
    free(banco.disponivel);
    return EXIT_SUCCESS;
}
