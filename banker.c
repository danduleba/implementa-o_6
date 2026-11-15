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

    for (size_t recurso = 0; recurso < banco->recursos; recurso++)
        trabalho[recurso] = banco->disponivel[recurso];

    size_t concluidos = 0;

    while (concluidos < banco->clientes) {
        bool avancou = false;

        for (size_t cliente = 0; cliente < banco->clientes; cliente++) {

            if (terminou[cliente])
                continue;

            size_t base = cliente * banco->recursos;
            bool pode_terminar = true;

            for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
                int falta = calcular_necessidade(banco->maximo[base + recurso],
                                                 banco->alocado[base + recurso]);
                if (falta > trabalho[recurso]) {
                    pode_terminar = false;
                    break;
                }
            }

            if (!pode_terminar)
                continue;

            for (size_t recurso = 0; recurso < banco->recursos; recurso++)
                trabalho[recurso] += banco->alocado[base + recurso];

            terminou[cliente] = true;
            concluidos++;
            avancou = true;
        }

        if (!avancou)
            break;
    }

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

void solicitar(Banco *banco, FILE *saida, size_t cliente,
                      const int *valores)
{

    size_t base = cliente * banco->recursos;

    for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
        int falta = calcular_necessidade(banco->maximo[base + recurso],
                                         banco->alocado[base + recurso]);
        if (valores[recurso] > falta) {
            fprintf(saida, "The customer %zu request ", cliente);
            escrever_vetor(saida, valores, banco->recursos);
            fputs(" was denied because exceed its maximum allocation\n", saida);
            return;
        }
    }

    for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
        if (valores[recurso] > banco->disponivel[recurso]) {
            fputs("The resources ", saida);
            escrever_vetor(saida, banco->disponivel, banco->recursos);
            fprintf(saida, " are not enough to customer %zu request ", cliente);
            escrever_vetor(saida, valores, banco->recursos);
            fputs(" \n", saida);
            return;
        }
    }

    for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
        banco->disponivel[recurso] -= valores[recurso];
        banco->alocado[base + recurso] += valores[recurso];
    }

    if (!estado_seguro(banco)) {

        for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
            banco->disponivel[recurso] += valores[recurso];
            banco->alocado[base + recurso] -= valores[recurso];
        }
        fprintf(saida, "The customer %zu request ", cliente);
        escrever_vetor(saida, valores, banco->recursos);
        fputs(" was denied because result in an unsafe state\n", saida);
        return;
    }

    fprintf(saida, "Allocate to customer %zu the resources ", cliente);
    escrever_vetor(saida, valores, banco->recursos);
    fputc('\n', saida);
}

void devolver(Banco *banco, FILE *saida, size_t cliente,
                     const int *valores)
{
    size_t base = cliente * banco->recursos;

    for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
        if (valores[recurso] > banco->alocado[base + recurso])
            erro("RL tenta devolver mais recursos do que o cliente possui");
    }

    for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
        banco->alocado[base + recurso] -= valores[recurso];
        banco->disponivel[recurso] += valores[recurso];
    }

    fprintf(saida, "Release from customer %zu the resources ", cliente);
    escrever_vetor(saida, valores, banco->recursos);
    fputc('\n', saida);
}

static void processar_comandos(Banco *banco, FILE *entrada, FILE *saida)
{

    char *linha = NULL;
    size_t capacidade = 0;

    int *valores = alocar(banco->recursos, sizeof *valores);

    while (ler_linha(entrada, &linha, &capacidade)) {
        char *p = pular_espacos(linha);

        if (*p == '*' && *pular_espacos(p + 1) == '\0') {
            mostrar_estado_inicial(banco, saida);
            continue;
        }

        if (strncmp(p, "RQ", 2) != 0 && strncmp(p, "RL", 2) != 0)
            erro("comando inválido em commands.txt");

        char operacao = p[1];
        p += 2;

        if (!isspace((unsigned char)*p))
            erro("separe os campos do comando por espaços");

        size_t cliente = (size_t)ler_inteiro(&p);
        if (cliente >= banco->clientes)
            erro("cliente inexistente em commands.txt");

        for (size_t recurso = 0; recurso < banco->recursos; recurso++) {
            if (!isspace((unsigned char)*p))
                erro("separe os campos do comando por espaços");
            valores[recurso] = ler_inteiro(&p);
        }

        if (*pular_espacos(p) != '\0')
            erro("quantidade de valores incorreta em commands.txt");

        if (operacao == 'Q')
            solicitar(banco, saida, cliente, valores);
        else
            devolver(banco, saida, cliente, valores);
    }

    free(valores);
    free(linha);
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

    FILE *entrada = abrir("commands.txt", "r");
    FILE *saida = abrir("result.txt", "w");

    processar_comandos(&banco, entrada, saida);

    int falha_entrada = fclose(entrada);
    int falha_saida = ferror(saida);
    int fechamento_saida = fclose(saida);

    if (falha_entrada != 0)
        erro("falha ao fechar commands.txt");
    if (falha_saida || fechamento_saida != 0)
        erro("falha ao gravar result.txt");

    free(banco.alocado);
    free(banco.maximo);
    free(banco.disponivel);
    return EXIT_SUCCESS;
}
