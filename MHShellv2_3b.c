/*************************************************************************************
   Arquivo:    "MHShellv2_3a.c" (Versao 2.3 Expande com comandos if e while)
   Descricao:  main do shell, Funcoes, Tipos e Variaveis Globais
               implementa abreviatura na MHplus
   Autor:      Marcelo Fagundes Felix
   Data:       04/06/2019
**************************************************************************************/

/*************************************************************************************

--- Alterações e acrescimos:
1. tron/trof
2. var = var
3. instrucao 7 (ci) STOV
4. display VAR e input com endereco da var visivel
5. Lembrar de controlar estouro nos vetores usados VAR, REG, nloop, MH, CI, etc (PENDENTE)

21/mar/19
6. Acrescimo das instrucoes ci 8 e 9 (ADDC e ADDV) com acrescimo em execCI();
7. Acrescimo do token '+=' (cod 11) para incremento de valor (var+=cte ou var+=var);
8. Acrescimo do token '//' (cod 80 ou 98) para marca de inicio de comentario em linha

9 acrescimo dos comandos:
mh    IN  var     instrucao   10 ci INP v
mh    OUT var     instrucao   11 ci OUT v

10. modificacao de comandos do shell (dspvar -> vars, dspmh -> mh, dspci -> ci e lang)

11. acrescimo de mais informacoes de debug no interpretador (nos "if (tron)") 18.abril
12. acrescimo dos tokens -- e -= com respectivas instruções MHplus

13. Suporte as Abreviaturas: ABBR RET e MAIN:
a. nocoes de escopo global, local (abbr e main) e lista de parametros formais
b. reconhecimento das novas formas de declaracoes
c. definicao de ABBR nome-abbr par-form1 par-form2 ... par-formK
d. reconhecimento do comando RET (com passagem de retorno no acumulador)
e. criacao de novo registrador acumulador AC (usado na traducao do comando RET)
f. traducao de RET usa duas instrucoes de AC novas: LDAC var e STAC var
   LDAC var equiv AC <- var
   STAC var equiv var <- AC
g. instrucao nova PSHA end que empilha end na PER (Pilha de enderecos de retorno)
h. instrucao nova POPA que carrega PC com o topo desempilhado da PER

--- Correcao de BUGs (beta 1.00, 01, 02)

1. nao reporta erro de "var nao declarada": case 1 de transMH2CI(): CORRIGIDO
2. Escapes (\t etc) nao estao sendo tratados: PENDENTE

3. geracao errada de endereco de JMP a partir de um 2o GOTO com rotulo ja instalado:
CORRIGIDO

4. sobra de GOTOs sem endci resolvido, sem report de erro sintatico: PENDENTE

5. 2o load da sessao dando erro de traducao nas ultimas linhas (para um mesmo mh)
quando o 1o funcionou perfeitamente antes: CORRIGIDO (ROT[0] nao inicializada)

6. quando reporta erro sintatico deve resetar o CI gerado parcialmente (e MH tb):
PENDENTE

7. Rever variaveis globais e suas reinicializacoes durante sessao: PENDENTE

8. Teste com gotoinloop.txt demonstra que salto para dentro de loop sem carga do REG
provoca erro no interpretador (entra em loop pois retorna necessariamente ao JMPZ)
SOLUCAO: inicializar REGs com 0 e so faz DEC se REG > 0
case 5 do execCI() CORRIGIDO

9. Se na ultima linha cod mh nao for dado ENTER, o analisador acusa "EOL expected".
Mas parece gerar codigo normalmente. PENDENTE

10. Acrescimo das instrucoes ci:

   DECC - decremento constante em variavel
   DECV - decremento variavel em variavel

11. Antiga DEC passa a se chamar DECR - decremento de registrador de controle.

12. Acrescentando os tokens *= /= e ^= mais op-rel nao contraidos

13. instrucoes ci MULC e MULV com respectiva geracao de código em transMH2CI e
maquina virtual MH (interpretador de CI)

14. ERRO detectado!!! LOOP sem END fechando: gerou codigo onde o JMPZ ficou sem endereco
de escape do LOOP. Executou normalmente o bloco (e ate loops internos)
CORRECAO: detectar !pilhaLAVazia() ==> "end expected"

15. NECESSIDADE DE APAGAR o ci gerado quando qq erro sintatico é produzido!!!!!!!!!
RESOLVIDO!!!

--- Bugs, observacoes e correcoes: MHplus com abreviaturas

16. Falha em pow de <math.h> no CodeBlocks GNU GCC: reimplementada em C;

17. Reset da Tabela VAR a cada load;

18. Variaveis globais nao pode ter homonimas locais. Isso poderia suscitar enganos
invonluntarios. Se houver homonima local, emite erro de redefinicao de id.

19. Se houver declaracao NAT global e comandos antes da 1a abbr ou do main, os comandos
estao tendo codigo gerado mas o 1 JMP para o main torna-os invisiveis. Solucao: novo
esquema para desvio para main - so pode definir onde estara o JMP no ci, na hora que
descobrir a 1a abbr. Analisar o que ocorre se nao tiver nem abbr nem main.

20. Se nao tem abbr nem main, esta gerando codigo com 1 JMP 2 para poder traduzir
codigo legado da versao 1.0.

21. Programas sem main tem todas as variaveis em um unico escopo global.


*****************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <math.h>
#include "anaLexMHv2_00.c"

#define NUMMAXINSTCI 500   // tamanho maximo do codigo intermediario

typedef struct
{  int cmd;    // codigo da instrucao CI
   int pr1;    // 1o parametro da instrucao CI
   int pr2;    // 2o parametro da instrucao CI
   int src;    // numero da linha do fonte MH para debug(indice para mh[ci[e].src]
} Tinstrucao;  // tipo das instrucoes CI

Tinstrucao CI[NUMMAXINSTCI];  // vetor que armazena o codigo intermediario
                              // o ci eh gerado na traducao do fonte .MH

int tron;   // controla a exibicao do trace (tron = "trace on", tron = 1 ou 0)
int color = 0;
int numErrosSintaticos = 0; // conta o numero de Erros Sintaticos na traducao

int e;   // endereco corrente do ci durante a traducao

typedef struct
{  int val;
   char nome[32]; // 12 tamanho maximo dos identificadores de var
} Tvar;           // estrutura que define as variaveis MH

typedef struct
{  int endci;     // endereco real em ci associado ao endereco simbolico id-rot
   char nome[12]; // nome do id-rot (endereco simbolico do fonte MH)
} Trot;           // struct que relaciona enderecos simbolicos MH com end fisicos ci

char MH[300][LIM];   // codigo fonte MH com um maximo de 300 linhas
int countLinMH;      // contador de linhas do fonte MH

Tvar VAR[50];  // Tabela para 50 variaveis com valor e nome da var
               // VAR[endereco].val e VAR[endereco].nome
               // VAR[0].val registra o numero de variaveis declaradas

Trot ROT[50];  // Tabela p/ 50 rotulos simbolicos
               // Rotulos sao identificacoes simbolicas para enderecos reais do ci
               // Ou seja, se um comando ci ocorre no endereco E do código intermed.,
               // nao precisamos nos referir ao valor numerico E quando usarmos GOTO.
               // Usamos um simbolo para isso.
               // Exemplo: Seja E o endereco 99 do ci. Chamando 99 de "label" podemos
               // fazer "GOTO label" ao inves de GOTO 99.
               // Para isso que usamos rotulos simbolicos.
               // ROT[0].endci registra o numero de rotulos encontrados no fonte

int REG[300];  // maximo de 100 registradores de controle de repeticao de LOOP-END
               // controle de um maximo de 300 LOOP-END por codigo fonte MH
               // REG[0] registra o numero de registradores (LOOPs) no fonte

int nloop;  // indica o numero de LOOPS processados ate um certo momento da traducao
            // serve para determinar o REG de controle para cada LOOP-END
            // Uso de nloop:     REG[nloop]=VAR[end("X")].val;
            // Esta atribuicao inicia o registrador de nloop com o valor da var "X"

int ulab;   // indica o topo da Pilha de Loops Abertos

typedef struct
{
   int end; // guarda o endereço de retorno do JMP do END para o JMPZ do LOOP aberto
   int reg; // guarda o endereço do registrador de controle do LOOP aberto
} TLoopAberto;

TLoopAberto LA[100]; // Pilha de LOOPS abertos (máximo de 100 LOOPs por programa MH)

//////////////////////////////////////////////////////////////////////////////////////
// Dados para Maquina Virtual MHPlus (extensao de MH pura)
// Estruturas de Dados para suporte à abreviaturas (ABBR)
// Extensao de MHplus q permite chamar abreviaturas (sem recursao direta ou indireta)
//////////////////////////////////////////////////////////////////////////////////////

int AC; // registrador de proposito geral usado para buffer entre abreviatura e caller
// usado em LDAC var como AC <- var e STAC var como var <- AC

// Pilha usada no ambiente de execucao dos codigos ci
int PER[100];  // Pilha para os enderecos de retorno para o chamador das abreviaturas.
               // Antes do desvio para a chamada, empilha o endereco de retorno aqui.
               // A instrucao RET, desempilha daqui e usa o endereco obtido para o salto.
               // A instrucao ci nova PSHA sem parametros usa o reg PC automaticamente.
               // A POPA empilha na PER o endereco do chamadar para onde voltar.
               // O salto se da automaticamente ao se moficicar o PC como efeito do PSHA.

int uERet;     // topo da pilha de Enderecos de Retorno (das chamadas de abbr)

typedef struct
{
   char nome[12]; // id-var da abreviatura
   int ei;        // endereco de inicio do código da abbr
   int ef;        // endereco de final do codigo da abbr
   int parmF[16]; // lista com os enderecos em VAR dos parametros formais da abbr
                  // parmF[0] indica o numero de parametros na lista
} TdefAbbr;

char escopo[12];  // guarda o nome da ultima abbr aberta (pode ser "main")

TdefAbbr TAbb[50];   // Tabela que registra os dados de definicao das abreviaturas
                     // suporta ate 50 definicoes de abreviaturas
                     // TAbb[0].ef indica o numero de abbr's instaladas na tabela

/*************************************************************************************
 Funcao para report de erro sintatico observado durante a traducao de cada linha mh
**************************************************************************************/
void sintaxError(char msg[])
{
   printf("%s\n", msg);
   numErrosSintaticos++; //contabiliza Erros achados durante traducao
}

/*************************************************************************************
 Calcula potenciacao no lugar de pow in math.h que apresenta falha no GNU GCC
**************************************************************************************/
int powMH(int a, int b){
int m;
   for(m = 1; b--; m *= a);
   return(m);
}

/*************************************************************************************
 Recebe um id composto da forma escopo.idvar
 Retorna apenas o idvar
**************************************************************************************/
char * idVarSemEscopo(char idComposto[25])
{
   char * id;
   char neutro[25] = ""; // elemento neutro da concatenacao
   const char ponto = '.';
   id = strchr(idComposto, ponto);
   if ( id != NULL ){ // achou um '.' em idComposto
      id++; // aponta para o proximo caracter
      //printf("escopo . %s\n", id);
      //return(strcat(neutro, id)); // retorna o proprio token obtido por ultimo
      return(id);
      // uso de strcat apenas para evitar retorno de ponteiro para variavel local
   }
   //printf("sem escopo ==> %s\n", idComposto);
   return(idComposto);
}

/*************************************************************************************
 Retorna nome composto escopo.idVar. Se Escopo == "" o nome composto nao tem prefixo.
 Em funcao disso eh preciso redimensionar ids:
 id-var id-rotulo e id-abbr no fonte tem 12 caracteres.
 Ao compor os ids na forma prefix.sufix, terao 24+1 caracteres. Essa forma composta so
 surge durante a traducao, na tabela VAR. Isso repercute na saida do tron.
**************************************************************************************/
char * idEscopoVar(char escop[12], char idVar[12])
{
   char a[25];// nomes compostos de id passarao a ter ate 25 caracteres

   //printf("%s\t%s\n", escop, idVar);
   strcpy(a,"");
   if (strcasecmp(escop,"")!=0)
   {
      strcpy(a, escop);
      strcat(a, ".");
      //printf("%s\n", strcat(a,idVar));
      return(strcat(a,idVar));
   }
   else
   {
      //printf("%s\n", idVar);
      return(idVar);
   }
}

/*************************************************************************************
FUNCOES PARA USO DAS TABELAS DE VARIAVEIS, REGISTRADORES E ROTULOS

- Instalacao de Variaveis declaradas: Nas linhas NAT uma lista de vars e dada para que
sejam instaladas em VAR[]. Cada var tem nome e valor. O endereco da var eh o indice do
vetor VAR[].
- Instalacao de Registradores:a cada "LOOP var", um registrador correspondente ao LOOP
eh instalado em REG[]. A correspondencia eh estabelecida pela variavel global "nloop".
- Instalacao de Rotulos: cada rotulo eh instalado em duas situacoes
      a) rotulo : cmd
      b) GOTO rotulo
- quando acha um token id-rotulo deve-se buscar na tabela ROT para ver se ja foi
mencionado: se ja esta rotulando uma linha acima entao eh erro!!! Por outro lado,
se ja houve uso do rotulo em intrucao GOTO entao ja se sabe o endereco fisico que
o rotulo simboliza, pois a instrucao GOTO ja foi C U I D A D O !!!!!! REVER.....
- Rotulos nao podem rotular mais de uma linha (senao haveria nao-determinismo) pq
GOTO nao pode desviar para n linhas!
- No entanto, "GOTO rotulo" pode aparecer n vezes no codigo, desde que "rotulo"
identifique uma única linha.
*/

/*************************************************************************************
Retorna o endereco da variavel nomeVar na Tabela de Variaveis VAR.
Repare que nomeVar pode estar na forma composta ou nao.
Se nao estiver, tenta procurar na forma de id var simples, sem escopo.
**************************************************************************************/
int endVar(char nomeVar[25])
{
   int i;
   for (  i = 1;
         (i <= VAR[0].val && strcmp(VAR[i].nome, nomeVar));
          i++ );
   // VAR[0].val registra o numero de vars instaladas na Tabela VAR
   if (i > VAR[0].val){ // entao nomeVar com escopo nao esta presente na Tabela
      nomeVar = idVarSemEscopo(nomeVar); // retira o escopo
      for (  i = 1;
         (i <= VAR[0].val && strcmp(VAR[i].nome, nomeVar));
          i++ );
      if (i > VAR[0].val) // entao definitivamente nao esta na Tabela
         return(0);  // 0 indica a nao presenca da variavel na Tabela
      else
         return(1); // entao achou a nomeVar sem escopo na 2a tentativa
   }
   else
      return(i);  // entao achou a nomeVar na posicao i na 1a tentativa
}

/*************************************************************************************
Reseta todas as variaveis instaladas na tabela de variaveis VAR
**************************************************************************************/
int resetVars()
{
   int i;
   for (  i = 1; i <= VAR[0].val; VAR[i++].val = 0);
}

/*************************************************************************************
Retorna 1 se variavel nomeV esta instalada na tabela de variaveis VAR
        0 senao esta.
**************************************************************************************/
int varInstalada(char nomeV[12])
{
   //printf("%d\n", endVar(nomeV));
   return(endVar(nomeV)!=0); // se o ender de nomeV eh 0 entao ele nao esta instalado
}

/*************************************************************************************
Procedimento para instalar variavel nomeV na tabela de variaveis VAR.
Caso já esteja instalada, então indica Erro de redefinição.
OBS!!!!! Seria melhor retornar um valor 0/1 e deixar a msg de erro para o tradutor
**************************************************************************************/
int instalaVar(char nomeV[12])
{
   if (! varInstalada(nomeV))  // entao instala na tabela VAR
      strcpy( VAR[++VAR[0].val].nome , nomeV ); // guarda o nome na Tabela VAR
      // VAR[0].val registra o numero atual de variaveis instaladas na tabela
   else
      sintaxError("Variable already defined.");
}

/*************************************************************************************
Retorna o endereco da abbr nomeAbbr na Tabela TAbb
**************************************************************************************/
int endAbbr(char nomeAbbr[12])
{
   int i;
   for (  i = 1;
         (i<=TAbb[0].ef && strcmp(TAbb[i].nome, nomeAbbr));
          i++ );
   // TAbb[0].ef registra o numero de abreviaturas instaladas na Tabela TAbb
   if (i>TAbb[0].ef)  // entao nomeAbbr nao esta presente na Tabela
      return(0);  // 0 indica que a abreviatura nao foi definida ainda
   else
      return(i);  // entao achou a abreviatura na posicao i
}

/*************************************************************************************
Retorna 1 se abbr nomeA esta instalada na tabela de abreviaturas TAbb
        0 senao esta.
**************************************************************************************/
int abbrInstalada(char nomeA[12])
{
   return(endAbbr(nomeA)!=0); // se o ender de nomeA eh 0 entao ele nao esta instalado
}

/*************************************************************************************
Procedimento para instalar abreviatura nomeA na tabela de TAbb.
Caso já esteja instalada, então indica Erro de redefinição.
OBS!!!!! Seria melhor retornar um valor 0/1 e deixar a msg de erro para o tradutor
**************************************************************************************/
int instalaAbbr(char nomeA[12])
{
   if (! abbrInstalada(nomeA))  // entao instala na tabela TAbb
      strcpy( TAbb[++TAbb[0].ef].nome , nomeA ); // guarda o nome na Tabela TAbb
      // TAbb[0].ef registra o numero atual de abreviaturas instaladas na tabela
   else
      sintaxError("Abbr already defined.");
}

/*************************************************************************************
Retorna o indice do id-rot dentro da Tabela de rotulos ROT
Retorna 0 se o nome do id-rot nao esta instalado na tabela
Independentemente do valor do endereco real (endci) do rotulo simbolico
**************************************************************************************/
int indiceROT(char nome[12])
{
   int i;
   for (  i = 1;
         (i<=ROT[0].endci && strcmp(ROT[i].nome, nome));
          i++ );
   // ROT[0].endci registra o numero de rotulos instalados na Tabela ROT
   if (i>ROT[0].endci)  // entao nome nao esta presente na Tabela ROT
      return(0);  // 0 indica a nao presenca do rotulo na Tabela
   else
      return(i);  // entao achou o rotulo na posicao i
}


/*************************************************************************************
Retorna se o rotulo nomeR esta presente ou nao em ROT independente do endci
**************************************************************************************/
int idrotInstalado(char nomeR[12])
{
   return(indiceROT(nomeR)!=0); // se indice de nomeR em ROT eh 0 entao nao instalado
}

/*************************************************************************************

**************************************************************************************/
int endResolvido(int i)
// Retorna 0 ou 1 conforme o endereco ci na posicao i de ROT seja 0 ou nao 0
{
   return(ROT[i].endci);
}

/*
int instalaRot(char nomeR[12], int endReal)
// Um rotulo so eh instalado em ROT quando surge numa linha rotulada "id-rot : cmd"
// Quando um id-rot surge pela 1a vez usado num comando GOTO, existem 2 casos:
//    1) a declaracao do rotulo "id-rot : cmd" surge em alguma linha mais adiante
//    2) nao existe a declaracao do rotulo id-rot e, nesse caso, o GOTO eh indefinido
// Neste caso, nao eh possivel, neste momento, determinar o end ci real para o desvio.
// O campo endci permanece zerado ate que surja a declaracao UNICA do rotulo.
// Quando uma declaracao "id-rot : cmd" surge pela 1a vez, eh possivel determinar o
// endereco ci real do cmd rotulado pelo id-rot. Esse endereco eh registrado na ROT
// e sera usado quando surgirem GOTOs para o id-rot.
// Como nao pode existir mais de uma linha rotulada pelo mesmo id-rot, caso o nomeRot
// ja se encontre instalado na tabela, devera ser reportado um erro de duplo destino.
{
   if (! idrotInstalado(nomeR)) // entao instala na tabela ROT
   {
      strcpy( ROT[++ROT[0].endci].nome, nomeR ); // guarda nomeR em ROT
      // ROT[0].endci registra o numero atual de rotulos instalados na tabela
      ROT[ROT[0].endci].endci = endReal;  // guarda o endereco ci real
   }
   else
      sintaxError("Duplicated label: non-determinism is not allowed.");
}
*/

/*************************************************************************************

**************************************************************************************/
// registra o endci em um rotulo ja instalado na tabela ROT
int resolveEndRot(int i, int endReal)
{
   ROT[i].endci = endReal;  // troca 0 pelo endereco real do comando rotulado no ci
}

/*************************************************************************************

**************************************************************************************/
int instalaRot(char nomeR[12], int endReal)
// Simplesmente instala o nome simbolico nomeR e o respectivo endereco real ci
// na proxima posicao livre da tabela ROT
{
   strcpy( ROT[++ROT[0].endci].nome, nomeR ); // guarda nomeR em ROT
   // ROT[0].endci eh incrementado para registrar o numero atual de rotulos instalados
   // ROT[0].endci eh usado para indexar a tabela ROT em seu ultimo rotulo instalado
   ROT[ROT[0].endci].endci = endReal;  // guarda o endereco ci real

   //instrumentacao
   //printf("instalou %s %d\n", ROT[ROT[0].endci].nome, ROT[ROT[0].endci].endci);
}



/*************************************************************************************

**************************************************************************************/
int resolveGOTOsAbertos()
{
   //instrumentacao
   //printf("Rotulos\n");
   //for(int i=1; i<=ROT[0].endci; printf("%d %s %d\n", i, ROT[i].nome,
   //                                                      ROT[i].endci), i++);

   for(int i=1; CI[i].cmd; i++)
      if (CI[i].cmd == 6 && CI[i].pr1==0) // precisa resolver este endereco de desvio
      {
         //instrumentacao
         //printf("%d\t%d\t%d\n", i, CI[i].pr1, CI[i].pr2);
         CI[i].pr1 = ROT[CI[i].pr2].endci;
         CI[i].pr2 = 0;
      }
}

/*************************************************************************************
   FUNCOES DA PILHA DE LOOPS ABERTOS que controla a traducao das estruturas LOOP-END
   A Sintaxe Sensível ao Contexto que requer Automato de Pilha + Reg de repeticao

   Empilha informacoes do ultimo LOOP aberto
   => e: endereco do JMPZ de controle das repeticoes do LOOP
   => r: registrador de controle do LOOP

**************************************************************************************/
void empilhaLA( int e, int r) // instala LOOP mais recente na pilha
{
   LA[++ulab].end = e;     // endereço do JMPZ do LOOP aberto
   LA[ulab].reg = r;       // registrador de controle de repeticoes do LOOP aberto
}

/*************************************************************************************

**************************************************************************************/
TLoopAberto desempilhaLA() // retorna o ultimo LOOP aberto, retirando da pilha
{
   TLoopAberto L;
   L.end = LA[ulab].end;
   L.reg = LA[ulab--].reg;
   return(L);
}

/*************************************************************************************

**************************************************************************************/
int pilhaLAvazia()
{
   return (ulab==0);
}

/*************************************************************************************
   FUNCOES DA PILHA DE ENDERECOS DE RETORNO que controla fluxo nas chamadas de abbr
**************************************************************************************/
void empilhaPER(int e) // instala endereco de retorno no topo da pilha
{
   PER[++uERet] = e;   // endereço da proxima instrucao a executar apos comando RET
}

/*************************************************************************************
Desempilha o ultimo endereco de retorno de chamada, retirando da pilha
**************************************************************************************/
int desempilhaPER()
{
   return(PER[uERet--]);
}

/*************************************************************************************
Retorna 1 ou 0 para pilha vazia ou nao.
**************************************************************************************/
int pilhaERvazia()
{
   return (uERet==0);
}

/*************************************************************************************
                  FUNCOES QUE IMPLEMENTAM OS COMANDOS DO SHELL
**************************************************************************************/

void apagaCI()
{
   CI[1].cmd = 0; // colocar um HLT na primeira instrucao serve para "apagar" o ci
}

/*************************************************************************************
Implementa o comando "ci" que exibe o codigo intermediario gerado, se existe
Deve notificar caso nao exista ci gerado pelo tradutor mh2ci
**************************************************************************************/
void displayCI()
{
   int c;
   for(int i=1; c = CI[i].cmd; i++) // termina quando encontra um cmd HALT
      // o CI comeca no endereco 1 e termina com cmd == 0
      switch(c)
      {
         case 0:
            break;
         case 1:
            printf("%4d: %s  %3d  %3d\t{%s", i, "STOC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 2:
            printf("%4d: %s  %3d  %s\t{%s", i, "INC ", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 3:
            printf("%4d: %s  %3d  %3d\t{%s", i, "CPYR", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 4:
            printf("%4d: %s  %3d  %3d\t{-\n", i, "JMPZ", CI[i].pr1, CI[i].pr2);
            break;
         case 5:
            printf("%4d: %s  %3d  %s\t{-\n", i, "DECR", CI[i].pr1, "   ");
            // DECR eh especifico para registradores de controle
            // nao confundir com DEC que implementa o cmd -- sobre variaveis
            // faz par com DECV, especifico para decremento variavel de variavel
            // e com DECC, especifico para decremento constante de variavel
            // obs: DECC var 1 equivale a DEC var (var--)
            break;
         case 6:
            printf("%4d: %s  %3d  %s\t{%s", i, "JMP ", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 7:
            printf("%4d: %s  %3d  %3d\t{%s", i, "STOV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 8:
            printf("%4d: %s  %3d  %3d\t{%s", i, "ADDC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 9:
            printf("%4d: %s  %3d  %3d\t{%s", i, "ADDV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 10:
            printf("%4d: %s  %3d  %s\t{%s", i, "INP ", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 11:
            printf("%4d: %s  %3d  %s\t{%s", i, "OUT ", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 12:
            printf("%4d: %s  %3d  %s\t{%s", i, "DEC ", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 13:
            printf("%4d: %s  %3d  %3d\t{%s", i, "DECC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 14:
            printf("%4d: %s  %3d  %3d\t{%s", i, "DECV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 15:
            printf("%4d: %s  %3d  %3d\t{%s", i, "MULC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 16:
            printf("%4d: %s  %3d  %3d\t{%s", i, "MULV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 17:
            printf("%4d: %s  %3d  %3d\t{%s", i, "DIVC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 18:
            printf("%4d: %s  %3d  %3d\t{%s", i, "DIVV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 19:
            printf("%4d: %s  %3d  %3d\t{%s", i, "POTC", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 20:
            printf("%4d: %s  %3d  %3d\t{%s", i, "POTV", CI[i].pr1, CI[i].pr2, MH[CI[i].src]);
            break;
         case 21:
            printf("%4d: %s  %3d  %s\t{%s", i, "PSHA", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 22:
            printf("%4d: %s  %s  %s\t{-\n", i, "POPA", "   ", "   ");
            break;
         case 23:
            printf("%4d: %s  %3d  %s\t{%s", i, "LDAC", CI[i].pr1, "   ", MH[CI[i].src]);
            break;
         case 24:
            printf("%4d: %s  %3d  %s\t{%s", i, "STAC", CI[i].pr1, "   ", MH[CI[i].src]);
            break;

      }
      printf("\n");
}

/*************************************************************************************
Lista o programa fonte MH na tela do shell
**************************************************************************************/
void displayMH()
{
   for (int i=1; i<countLinMH; printf("%4d: %s", i, MH[i]), i++);
}

/*************************************************************************************
Exibe variaveis do ambiente da sessao
**************************************************************************************/
void displayVAR()
{
   for(int i=1; i<=VAR[0].val;
               printf(" [%d] %s = %d\n", i, VAR[i].nome, VAR[i].val), i++);
      // VAR[0] guarda o numero de vars na tabela de variaveis VAR
}

/*************************************************************************************
Exibe valores dos  REGs usados na sessao
**************************************************************************************/
void displayREG()
{
   for(int i=1; i<REG[0]; printf("REG[%d] = %d\n", i, REG[i]), i++);
}

/*************************************************************************************
Pede via teclado os valores das variaveis da sessao
**************************************************************************************/
void carregaVAR()
{
   int i=1, x=0;
   while(i<=VAR[0].val)
   {
      printf(" [%d] %s = ", i, VAR[i].nome);
      scanf("%d", &x);
      VAR[i++].val=x;
   }
}

/*************************************************************************************
Traduz uma linha contendo comandos MH para comando CI correspondente

Codigos Tokens MU PURA:
1     id-var
2     id-rot
3     ++
4     =
5     Numero
6     :
7     NAT
8     LOOP
9     END
10    GOTO
0     ? (indeterminado)
-1    indefinido (token fora de MH)

97    \t (tabulacao)
98    \n (fim de linha)
99    \0 (fim de string)

TOKENS de MH PLUS
11    +=
12    --
13    -=
14    *=
15    /=
16    ^= op contraido de potencia

as formas contraidas op= sao binarias acumulantes tipo x = x op y
se um op eh unario, basta x = ! y
relacionais nao sao acumulantes... T = a op-rel b


31    ==
32    !=
33    <
34    >
35    <=
36    >=
37    &&
38    ||
39    !

75    IN
76    OUT

80    //       comentario ate o fim da linha (identico a um \n)

Formatos para uma linha de cmd MH
         NAT x x x ...
         LOOP x
         END
         GOTO r
         r : cmd
         x = num
         x = y
         x ++
         x --

         x += num
         x += y

         x -= num
         x -= y

         IN x
         OUT x

         ============= Abreviaturas (definicao e chamada)
         ABBR id-abbr parmformal1 parmformal2 ...
         RET id-var

         MAIN

         id-var = id-abbr parmreal1 parmreal2 ...
         ================================================

Recebe a linha do fonte MH a ser traduzida
*/

/*************************************************************************************
Traduz uma linha do arquivo .mh de entrada, gerando respectivas instruções ci.
A linha do arquivo esta previamente armazenada no vetor MH global.
As instrucoes ci sao instaladas no vetor CI global que sera usado pelo interpretador.
**************************************************************************************/
void transMH2CI(int linMH)
{
   Ttok tok;
   char tokEscopoVar[25];
   int pos;
   int eVdest, eVorig, iR, eAbb;
   TLoopAberto LoopAberto;

   pos = 0;       // posicao do caracter a ser observado na analise de tokens

   tok = getTok(&pos, MH[linMH]);

   if (tok.cod == 2) // Se a linMH inicia com um "id-rot"
   {
      iR = indiceROT(tok.val);   // retorna posicao em ROT caso rotulo instalado
      if (iR == 0)  // rotulo nao instalado na tabela ROT
         // entao instala na tabela ROT o nome e o end real e+1 do rotulo em tok
         instalaRot(tok.val, e+1);  // e+1 eh o ender do cmd rotulado por esse rotulo
      else // o rotulo esta instalado, com ou sem o endereco resolvido
         if (endResolvido(iR)) // entao eh um rotulo duplicado e eh erro
            sintaxError("Duplicated label: non-determinism is not allowed.");
         else  // endereco real do rotulo nao esta resolvido
            resolveEndRot(iR, e+1); // resolve usando end da prox instruc a ser gerada

      // pega o proximo tok que deve necessariamente ser um ':'
      tok = getTok(&pos, MH[linMH]);
      if (tok.cod == 6) // se ':', pega o prox tok (inicio do cmd apos o rotulo)
      {
         tok = getTok(&pos, MH[linMH]);
         // pega proximo tok e desvia para o switch para tratar o tok depois do ':'
         goto trataLinhaCmdSemRotulo;
      }
      else sintaxError("Token ':' expected."); // poderia desinstalar o rotulo
   }

   else
   // A linha MH sendo traduzida nao eh um comando rotulado.
   // Segue com a traducao de uma linha de cmd MH sem rotulo.
   {
      trataLinhaCmdSemRotulo:
      switch(tok.cod)
      {
         case 1:  // id-var
            strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
            eVdest = endVar(tokEscopoVar);  // define o endereco da var destino

            if(eVdest)  // se var foi declarada segue com a traducao
            {
               tok = getTok(&pos, MH[linMH]);   // deve ser um =, um ++, um +=, um -- ou um -=
               if (tok.cod == 4) // tok eh '='
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                                                   // ou chamada a uma ABBR
                  if (tok.cod == 5) // se tok eh cte Numerica
                  {
                     // gera um ci atribuicao  var = num
                     e++;
                     CI[e].cmd = 1;    // STOC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // se tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        // gera um ci atribuicao var_destino = var_origem
                        e++;
                        CI[e].cmd = 7;       // STOV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected.");
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else if (tok.cod == 77) // se tok eh id-abbr entao eh chamada
                  {
                     eAbb = endAbbr(tok.val);  // obtem entrada Tab de Abreviaturas TAbb
                     if(eAbb) // se a abbr foi definida entao gera a chamada
                     {
                        // gera os STOVs para passar cada parametro real para os formais da abbr
                        int i = 1; // indice do parametro formal na lista de parms da abreviatura
                        int eParmR = 0, eParmF = 0;
                        while ((tok = getTok(&pos, MH[linMH])), (tok.cod == 1)) // para cada id-var
                        {
                           // gera STOV eParmF eParmR
                           strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                           eParmR = endVar(tokEscopoVar);
                           eParmF = TAbb[eAbb].parmF[i++]; // endereco VAR do parmF da abbr
                           e++;
                           CI[e].cmd = 7;       // STOV eParmF eParmR
                           CI[e].pr1 = eParmF;  // destino
                           CI[e].pr2 = eParmR;  // origem
                           CI[e].src = linMH;
                           //iparmF++;
                        }
                        i--; // desconta ultimo ++ para comparar num de parmR com num de parmF
                        if (i != TAbb[eAbb].parmF[0]) // posicao 0 de parmF guarda o numero de parms formais
                           sintaxError("Parameters mismatch.");
                        // gera PSHA e+2
                        e++;
                        CI[e].cmd = 21;
                        CI[e].pr1 = e+2;
                        CI[e].pr2 = 0;
                        CI[e].src = linMH;
                        // gera JMP
                        e++;
                        CI[e].cmd = 6;
                        CI[e].pr1 = TAbb[eAbb].ei; // salto para o inicio do codigo da Abbr
                        CI[e].pr2 = 0;
                        CI[e].src = linMH;
                        // gera STAC eVdest
                        e++;
                        CI[e].cmd = 24;
                        CI[e].pr1 = eVdest; // armazena o conteudo de AC na variavel destino
                        CI[e].pr2 = 0;
                        CI[e].src = linMH;

                        if (tok.cod == 98) // apos lista de parms reais tok deve ser '\n'
                           break;
                        else
                           sintaxError("EOL expected.");
                     }
                     else
                        sintaxError("Undefined Abbr.");
                  }
                  else
                     sintaxError("Constant, variable or abbr expected.");
               }
               else if (tok.cod == 3)  // tok eh '++'
               {
                  e++;
                  CI[e].cmd = 2; // INC eVdest
                  CI[e].pr1 = eVdest;
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;
                  tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                  if (tok.cod == 98) // tok eh '\n'
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else if (tok.cod == 11)  // tok eh '+='
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                  if (tok.cod == 5) // tok eh cte Numerica
                  {
                     e++;
                     CI[e].cmd = 8;    // ADDC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        e++;
                        CI[e].cmd = 9;       // ADDV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected."); //     Cuidado!!!!!!
                           // \t e outros escapes no fonte nao sao permitidos
                           //a menos que no pulaBrancos eles sejam pulados tb.
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else
                     sintaxError("Constant or variable expected.");
               }
               else if (tok.cod == 12)  // tok eh '--'
               {
                  e++;
                  CI[e].cmd = 12; // DEC eVdest
                  CI[e].pr1 = eVdest;
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;
                  tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                  if (tok.cod == 98) // tok eh '\n'
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else if (tok.cod == 13)  // tok eh '-='
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                  if (tok.cod == 5) // tok eh cte Numerica
                  {
                     e++;
                     CI[e].cmd = 13;    // DECC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        e++;
                        CI[e].cmd = 14;       // DECV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected."); //     Cuidado!!!!!!
                           // \t e outros escapes no fonte nao sao permitidos
                           //a menos que no pulaBrancos eles sejam pulados tb.
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else
                     sintaxError("Constant or variable expected.");
               }
               else if (tok.cod == 14) // tok eh "*="
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                  if (tok.cod == 5) // tok eh cte Numerica
                  {
                     e++;
                     CI[e].cmd = 15;    // MULC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        e++;
                        CI[e].cmd = 16;       // MULV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected."); //     Cuidado!!!!!!
                           // \t e outros escapes no fonte nao sao permitidos
                           //a menos que no pulaBrancos eles sejam pulados tb.
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else
                     sintaxError("Constant or variable expected.");
               }
               else if (tok.cod == 15) // tok eh "/="
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                  if (tok.cod == 5) // tok eh cte Numerica
                  {
                     e++;
                     CI[e].cmd = 17;    // DIVC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        e++;
                        CI[e].cmd = 18;       // DIVV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected."); //     Cuidado!!!!!!
                           // \t e outros escapes no fonte nao sao permitidos
                           //a menos que no pulaBrancos eles sejam pulados tb.
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else
                     sintaxError("Constant or variable expected.");
               }

               else if (tok.cod == 16) // tok eh "^="
               {
                  tok = getTok(&pos, MH[linMH]);   // pode ser uma cte numerica ou outro id-var
                  if (tok.cod == 5) // tok eh cte Numerica
                  {
                     e++;
                     CI[e].cmd = 19;    // POTC var cteNum
                     CI[e].pr1 = eVdest;   // endereco da var destino
                     CI[e].pr2 = atoi(tok.val);  // valor int da cte numerica
                     CI[e].src = linMH;
                     tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                     if (tok.cod == 98) // tok eh '\n'
                        break;
                     else
                        sintaxError("EOL expected.");
                  }
                  else if (tok.cod == 1) // tok eh outro id-var
                  {
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     eVorig = endVar(tokEscopoVar);  // obtem end var origem se declarada
                     if(eVorig)
                     {
                        e++;
                        CI[e].cmd = 20;       // POTV eVdest eVorig
                        CI[e].pr1 = eVdest;  // endereco da var destino
                        CI[e].pr2 = eVorig;  // endereco da var origem
                        CI[e].src = linMH;
                        tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                        if (tok.cod == 98) // tok eh '\n'
                           break;
                        else
                           sintaxError("EOL expected."); //     Cuidado!!!!!!
                           // \t e outros escapes no fonte nao sao permitidos
                           //a menos que no pulaBrancos eles sejam pulados tb.
                     }
                     else
                        sintaxError("Source variable not declared");
                  }
                  else
                     sintaxError("Constant or variable expected.");
               }
               else
                  sintaxError("Unexpected token.");
               break;
            }
            else
               sintaxError("Destination variable not declared.");

         case 7:  // NAT pode ocorrer em qq linha do fonte mh. Preferivel na 1a linha

            tok = getTok(&pos, MH[linMH]);   // eh preciso ter pelo menos 1 variavel
            if (tok.cod == 1) // tok eh id-var
            {
               //printf("======>>> %s\t%s\n", escopo, tok.val);
               strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
               instalaVar(tokEscopoVar);
            }
            else
            {
               sintaxError("Expected at least one id variable.");
               break; // fim da traducao dessa linha MH
            }
            do // segue instalando as demais variaveis caso existam
            {
               tok = getTok(&pos, MH[linMH]); // demais variaveis do NAT
               //printf("%s", tok.val);
               if (tok.cod == 1) // tok eh id-var
               {
                  strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                  instalaVar(tokEscopoVar);
               }
               else if (tok.cod == 98) // tok eh fim de linha '\n'
                  break;   // termino bem sucedido da traducao
               else
               {
                  sintaxError("Expected another variable.");
                  break;
               }
            }while(1);
            break;

         case 8:  // LOOP
            nloop++;
            tok = getTok(&pos, MH[linMH]);   // deve ser um id-var
            if (tok.cod == 1) // tok eh variavel
            {
               // printf("%s\n", tok.val);
               strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
               if (varInstalada(tokEscopoVar))
               {
                  // gerar um ci 3 CPYR
                  e++;
                  CI[e].cmd = 3;
                  CI[e].pr1 = nloop;   // registrador de controle deste LOOP
                  CI[e].pr2 = endVar(tokEscopoVar); // endereco da var em VAR
                  CI[e].src = linMH;   // linha do fonte desse LOOP (p/ uso de debug)
                  // gerar um ci 4 JMPZ
                  e++;
                  CI[e].cmd = 4;
                  CI[e].pr1 = nloop;
                  CI[e].pr2 = 0; // ainda nao podemos determinar o endereco do salto
                  CI[e].src = linMH;
                  // empilha informacoes deste LOOP
                  empilhaLA(e, nloop); // endereco que ficou aberto e reg desse LOOP
                  tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                  if (tok.cod == 98)
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else sintaxError("Undefined id variable.");
            }
            else sintaxError("Expected id variable.");
            break;

         case 9:  // END
            if (pilhaLAvazia())
               sintaxError("END without a LOOP openned.");
            else
            {
               LoopAberto = desempilhaLA();
               // gera DECR
               e++;
               CI[e].cmd = 5;
               CI[e].pr1 = LoopAberto.reg;   // registrador de controle do LOOP aberto
               CI[e].pr2 = 0;                // nao tem 2o parametro
               CI[e].src = linMH;            // linha do fonte mh desse END
               // gerar um JMP incondicional
               e++;
               CI[e].cmd = 6;
               CI[e].pr1 = LoopAberto.end;
               CI[e].pr2 = 0; // ainda nao podemos determinar o endereco do salto
               CI[e].src = linMH;
               // completa o JMPZ com endereco de salto em aberto
               CI[LoopAberto.end].pr2 = e+1;

               tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
               if (tok.cod == 98)
                  break;
               else
                  sintaxError("EOL expected.");
            }
            break;

         case 10: // GOTO
            tok = getTok(&pos, MH[linMH]);   // proximo tok precisa ser id-rot
            if (tok.cod == 2) // tok eh id-rot
            {
               iR = indiceROT(tok.val);
               if (iR == 0) // rotulo ainda nao instalado
               {
                  instalaRot(tok.val, 0); // instala com endereco ci nao resolvido
                  e++;
                  CI[e].cmd = 6;
                  CI[e].pr1 = 0; // indica JMP com endereco de desvio nao resolvido
                  CI[e].pr2 = ROT[0].endci; // guarda indice do ultimo rot instalado.
                  // Com este indice sera possivel, apos resolver todos os enderecos
                  // reais de cada rotulo do programa, voltar em CI resolvendo os
                  // desvios dos JMPs gerados e que ficaram em aberto
                  CI[e].src = linMH;
               }
               else  // rotulo ja esta instalado, com ou sem endereco ci resolvido
               {
                  if(endResolvido(iR)) // rot ja instalado e com end resolvido em ROT
                  {
                     e++;
                     CI[e].cmd = 6; // JMP
                     CI[e].pr1 = ROT[iR].endci; // resolve endereco de desvio do JMP
                     CI[e].pr2 = 0;
                     CI[e].src = linMH;
                  }
                  else  // rotulo instalado mas nao tem endereco real resolvido em ROT
                  {
                     // nao precisa instalar o rotulo pq ele ja esta instalado em ROT
                     e++;
                     CI[e].cmd = 6;
                     CI[e].pr1 = 0;
                     CI[e].pr2 = iR; // guarda o indice do rotulo para poder obter
                     // o endci desse rotulo quando estiver disponivel em ROT
                     CI[e].src = linMH;
                  }
               }
               tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
               if (tok.cod == 98)
                  break;
               else
                  sintaxError("EOL expected.");
            }
            else
               sintaxError("Label expected after GOTO.");

            break;

         case TOK_PAL_RES_ABBR: // definicao   abbr id-abbr id-var id-var ...

            tok = getTok(&pos, MH[linMH]);   // proximo token deve ser um id-abbr
            if (tok.cod == TOK_ID_ABREV) // tok eh identificador de abreviatura
            {
               if (strcasecmp(escopo, "") != 0) // se tem escopo tem ef pendente
               {
                  eAbb = endAbbr(escopo); // pega o endereco da abbr anterior na TAbb
                  TAbb[eAbb].ef = e;   // fecha o range do escopo da abbr que encerrou
               }

               strcpy(escopo, tok.val);   // muda o escopo para o nome da abbr atual
               instalaAbbr(tok.val);
               eAbb = endAbbr(tok.val);

               int i = 0; // indice da lista de parametros formais da Abbr
               do // segue instalando os parametros formais caso existam
               {
                  tok = getTok(&pos, MH[linMH]);

                  if (tok.cod == 1) // tok eh id-var
                  {
                     //printf("%s\t\t%s\n", escopo, tok.val);
                     strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
                     //printf("%s\n", tokEscopoVar);
                     instalaVar(tokEscopoVar); // instala o parmF em VAR com o prefixo
                     // registra o endereco do parm formal na entrada da abbr em TAbb
                     TAbb[eAbb].parmF[++i] = endVar(tokEscopoVar);
                  }
                  else if (tok.cod == 98) // tok eh fim de linha '\n'
                  {
                     TAbb[eAbb].parmF[0] = i; // guarda o numero de parms instalados
                     break;   // termino bem sucedido da traducao
                  }
                  else
                  {
                     sintaxError("Expected another variable.");
                     break;
                  }
               }while(1);
               // range de enderecos ci do escopo da abreviatura [ei,ef]
               TAbb[eAbb].ei = e + 1; // proximo endereco ci a ser gerado eh o inicio da abbr
               break;
            }
            else sintaxError("Abbr name id expected.");
            break;

         case TOK_PAL_RES_RET: // retorno de abbr:  RET id-var

            tok = getTok(&pos, MH[linMH]);   // proximo token deve ser um id-var
            if (tok.cod == TOK_ID_VAR) // tok eh id-var
            {
               if (strcasecmp(escopo, "") != 0) // se tem escopo gera
               {
                  strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val)); // monta id
                  e++;
                  CI[e].cmd = 23;   // LDAC
                  CI[e].pr1 = endVar(tokEscopoVar); // endereco da variavel de retorno
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;
                  e++;
                  CI[e].cmd = 22;   // POPA {PC <- DesempPER}
                  CI[e].pr1 = 0;    // nao tem parametros
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;
                  tok = getTok(&pos, MH[linMH]);   // prox tok deve ser um '\n'
                  if (tok.cod == 98)
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else
               {
                  sintaxError("Return not allowed out of abbr.");
                  break;
               }
            }
            else sintaxError("Expected id variable.");
            break;

         case TOK_PAL_RES_MAIN: // marca de inicio do principal

            tok = getTok(&pos, MH[linMH]);   // proximo token deve ser um '\n'
            if (tok.cod == TOK_ENDOFLINE)
            {
               if (strcasecmp(escopo, "") != 0) // se tem escopo tem ef pendente
               {
                  eAbb = endAbbr(escopo); // pega o endereco da abbr anterior na TAbb
                  TAbb[eAbb].ef = e;   // registra o fim do escopo da abbr anterior
               }

               strcpy(escopo, "main");   // muda o escopo para "main"

               CI[1].pr1 = e + 1;   // termina o JMP para o inicio do MAIN
               CI[1].pr2 = 0;
               CI[1].src = linMH;

               break;
            }
            else
               sintaxError("EOL expected.");
            break;

         case TOK_PAL_RES_IN: // IN var

            tok = getTok(&pos, MH[linMH]);   // proximo token deve ser um id-var
            if (tok.cod == 1) // tok eh variavel
            {
               //printf("======>>> %s\t%s\n", escopo, tok.val);
               strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
               if (varInstalada(tokEscopoVar))
               {
                  // gerar um ci 10 INP var
                  e++;
                  CI[e].cmd = 10;
                  CI[e].pr1 = endVar(tokEscopoVar); // endereco da var em VAR
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;   // linha do fonte p/ uso de debug
                  tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                  if (tok.cod == 98)
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else sintaxError("Undefined id variable");
            }
            else sintaxError("Variable expected.");
            break;

         case TOK_PAL_RES_OUT: // OUT var
            tok = getTok(&pos, MH[linMH]);   // proximo token deve ser um id-var
            if (tok.cod == 1) // tok eh variavel
            {
               // printf("%s\n", tok.val);
               strcpy(tokEscopoVar, idEscopoVar(escopo, tok.val));
               if (varInstalada(tokEscopoVar))
               {
                  // gerar um ci 11 OUT var
                  e++;
                  CI[e].cmd = 11;
                  CI[e].pr1 = endVar(tokEscopoVar); // endereco da var em VAR
                  CI[e].pr2 = 0;
                  CI[e].src = linMH;   // linha do fonte p/ uso de debug
                  tok = getTok(&pos, MH[linMH]);   // deve ser um '\n'
                  if (tok.cod == 98)
                     break;
                  else
                     sintaxError("EOL expected.");
               }
               else sintaxError("Undefined id variable");
            }
            else sintaxError("Expected id variable.");
            break;

         case 80:
            while(tok.cod != TOK_ENDOFLINE)
               tok = getTok(&pos, MH[linMH]);
            break;

         case -1: // fora de MH
            sintaxError("Invalid token.");
            break;

      }  // switch
   }     // else
}        // principal

/*************************************************************************************
 Implementa o comando "load" do shell
 Carga e traducao do fonte MH. Gera codigo intermediario ci
 LE O ARQUIVO FONTE SEPARANDO TOKENS E GERANDO INSTRUCOES CI
**************************************************************************************/
int loadMH()
{
   FILE *arq;
   int pos;
   Ttok tok;
   char nomeArq[16];
   // char escopo[12]; ERRO - deve ser global

   // tokAnterior.cod = -1;   // Usado para determinar "GOTO id-rotulo"
   // transferido para o analisador lexico

   nloop = 0;        // contador dos LOOPs do fonte mh
   ulab = 0;         // situacao inicial do topo da pilha de LOOPs abertos
   e = 1;            // endereco do codigo intermediario a ser gerado
   VAR[0].val = 0;   // nenhuma variavel instalada
   ROT[0].endci = 0; // nenhum rotulo instalado
   TAbb[0].ef = 0;   // nenhuma abbr instalada
   strcpy(escopo,"");// nenhum escopo de abbr aberto
   CI[e].cmd = 6;    // JMP para inicio do modulo MAIN - novo padrao de ci da ver 2.0
   CI[e].src = 1;

   numErrosSintaticos = 0; // ao inicio da traducao eh resetado para consultar depois

   printf("Source file: ");
   gets(nomeArq);
   arq = fopen(nomeArq, "r");
   if (arq == NULL)
   {
      printf("Error openning file!\n");
      return(1);
   }
   else
   {
      countLinMH=1; // contador de linhas do arquivo fonte
      while(fgets(MH[countLinMH], LIM, arq)) //Carrega cada linha
      {
         printf(" (%3d): %s", strlen(MH[countLinMH]), MH[countLinMH]);
         transMH2CI(countLinMH);
         countLinMH++;
      }

      if (!pilhaLAvazia())
      {
         sintaxError("LOOP openned without END expected.");
      }

      printf("\n %d lines translated.", countLinMH-1);
      if (numErrosSintaticos) // se teve erro sintatico durante a traducao
      {
         printf("\n %d error(s) found.", numErrosSintaticos);
         printf("\n Intermediate Code NOT generated.\n");
         //apagaCI();
      }
      else
      {
         CI[++e].cmd = 0; // acrescenta um comando HLT no final do ci
         resolveGOTOsAbertos();// com ABBR, GOTO precisa respeitar escopos onde ocorrem
         if (strcasecmp(escopo,"")==0) CI[1].pr1 = 2; // termina "1 JMP 2" para cod legados MHv1.x
         resetVars();
         printf("\n Source succesfully loaded.\n Intermediate Code generated.\n");
      }

      fclose(arq);
   }
}

/************************************************************************************
 Implementa o comando "run" do shell
 Maquina Virtual MH (INTERPRETADOR de CI)
 SO PODE SER ACIONADO CASO HAJA CI GERADO PELO TRADUTOR mh2ci
*/

int execCI()
{
   int PC;
   char c;
   int eRetorno = 0;
   int PCantes = 0;

   if (CI[1].cmd == 0) // termina caso nao haja ci carregado
   {
      printf("\n There is not a ci.\n");
      return 0;
   }

   for(int i = 1; i<=300; REG[i] = 0, i++);  // inicia todos os registradores

   PC=1;
   uERet = 0; // esvazia pilha de retorno de chamadas de abbr

   //tron=1;
   if (tron) printf("Type <ENTER> to continue and 'x<ENTER>' to stop process.\n");
   while(CI[PC].cmd) // para o loop de execucao quando cmd == 0 (comando HLT)
   {
      //if (tron) printf(" PC=%d\n",PC);
      switch(CI[PC].cmd)
      {
         case 1: // STOC var num
            VAR[CI[PC].pr1].val = CI[PC].pr2;
            if (tron)
            {
               printf(" %d: STOC %s %d\t{%s", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 2: // INC var
            VAR[CI[PC].pr1].val++;
            if (tron)
            {
               printf(" %d: INC %s\t{%s", PC, VAR[CI[PC].pr1].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 3: // CPYR reg var
            REG[CI[PC].pr1] = VAR[CI[PC].pr2].val;
            if (tron)
            {
               printf(" %d: CPYR %d %s\t{%s", PC, CI[PC].pr1, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" REG[%d] == %d\n",  CI[PC].pr1, REG[CI[PC].pr1]);
            }
            break;

         case 4: // JMPZ reg rot

            if (REG[ CI[PC].pr1 ] == 0) // se o reg indicado na instrucao em PC for 0 muda o PC
            {
               int PCant = PC; // salva o PC desta instrucao
               PC = (CI[PC].pr2)-1; // entao muda o PC para saltar para o endereco indicado pelo rotulo do JMPZ
                                    // um a menos porque sempre incrementa o PC depois do switch
               if (tron)
               {
                  printf(" %d: JMPZ REG[%d]zerado %d\t{%s", PCant, CI[PCant].pr1, CI[PCant].pr2, MH[CI[PCant].src]);
                  printf(" PC == %d\n", PC+1);
               }
            }
            else if (tron)
            {
               printf(" %d: JMPZ REG[%d]naozerado %d\t{%s", PC, CI[PC].pr1, CI[PC].pr2, MH[CI[PC].src]);
               printf(" PC == %d\n", PC);
            }
            break;

         case 5: // DECR reg
            if (REG[CI[PC].pr1])
            {
               REG[CI[PC].pr1]--;
               if (tron)
               {
                  printf(" %d: DECR REG[%d]\t{%s", PC, CI[PC].pr1, MH[CI[PC].src]);
                  printf(" REG[%d] == %d\n",  CI[PC].pr1, REG[CI[PC].pr1]);
               }
            }
            else  // com registrador nulo nao pode executar DEC
                  // isso so pode ocorrer no caso de um goto para um END sem REG carregado
               if (tron)
                  printf(" %d: DEC REG[%d]==0\t{%s", PC, CI[PC].pr1, MH[CI[PC].src]);
            break;

         case 6: // JMP rot
            if (tron)
               printf(" %d: JMP %d\t{%s\n", PC, CI[PC].pr1, MH[CI[PC].src]);
            PC = (CI[PC].pr1)-1; // muda o PC incondicionalmente
            break;

         case 7: // STOV varDest varOrig
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr2].val;
            if (tron)
            {
               printf(" %d: STOV %s %s\t{%s", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 8: // ADDC varDest num
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val + CI[PC].pr2;
            if (tron)
            {
               printf(" %d: ADDC %s %d\t{%s\n", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 9: // ADDV varDest varOrig
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val + VAR[CI[PC].pr2].val;
            if (tron)
            {
               printf(" %d: ADDV %s %s\t{%s\n", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 10: // INP var
            if (tron)
               printf(" %d: INP %s\n", PC, VAR[CI[PC].pr1].nome);
            printf(" %s = ", VAR[CI[PC].pr1].nome);
            scanf("%d", &VAR[CI[PC].pr1].val);
            break;

         case 11: // OUT var
            if (tron)
               printf(" %d: OUT %s\n", PC, VAR[CI[PC].pr1].nome);
            printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            break;

         case 12: // DEC var
            // #define MODULO(x) ((x)>=0?(x):-(x))
            // x = VAR[CI[PC].pr1].val;
            // x--;
            // VAR[CI[PC].pr1].val = MODULO(x);

            VAR[CI[PC].pr1].val--;
            if (VAR[CI[PC].pr1].val<0) VAR[CI[PC].pr1].val = 0; // "monus" em NAT
            if (tron)
            {
               printf(" %d: DEC %s\t{%s", PC, VAR[CI[PC].pr1].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 13: // DECC varDest num
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val - CI[PC].pr2;
            if (VAR[CI[PC].pr1].val<0) VAR[CI[PC].pr1].val = 0; // "monus" em NAT
            if (tron)
            {
               printf(" %d: DECC %s %d\t{%s\n", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 14: // DECV varDest varOrig
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val - VAR[CI[PC].pr2].val;
            if (VAR[CI[PC].pr1].val<0) VAR[CI[PC].pr1].val = 0; // "monus" em NAT
            if (tron)
            {
               printf(" %d: DECV %s %s\t{%s\n", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 15: // MULC varDest num
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val * CI[PC].pr2;
            if (tron)
            {
               printf(" %d: MULC %s %d\t{%s\n", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 16: // MULV varDest varOrig
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val * VAR[CI[PC].pr2].val;
            if (tron)
            {
               printf(" %d: MULV %s %s\t{%s\n", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 17: // DIVC varDest num
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val / CI[PC].pr2;
            if (tron)
            {
               printf(" %d: DIVC %s %d\t{%s\n", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 18: // DIVV varDest varOrig
            VAR[CI[PC].pr1].val = VAR[CI[PC].pr1].val / VAR[CI[PC].pr2].val;
            if (tron)
            {
               printf(" %d: DIVV %s %s\t{%s\n", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 19: // POTC varDest num
            VAR[CI[PC].pr1].val = powMH(VAR[CI[PC].pr1].val , CI[PC].pr2);
            if (tron)
            {
               printf(" %d: POTC %s %d\t{%s\n", PC, VAR[CI[PC].pr1].nome, CI[PC].pr2, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 20: // POTV varDest varOrig
            VAR[CI[PC].pr1].val = powMH(VAR[CI[PC].pr1].val , VAR[CI[PC].pr2].val);
            if (tron)
            {
               printf(" %d: POTV %s %s\t{%s\n", PC, VAR[CI[PC].pr1].nome, VAR[CI[PC].pr2].nome, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

         case 21: // PSHA end
            empilhaPER(CI[PC].pr1);
            if (tron)
               printf(" %d: PSHA %d\t{%s\n", PC, CI[PC].pr1, MH[CI[PC].src]);
            break;

         case 22: // POPA (nao tem parametros)
            //int eRetorno = 0;
            eRetorno = desempilhaPER();
            PCantes = PC;
            PC = eRetorno - 1; // apos o switch sempre incrementa
            // provoca salto automatico para o ponto de retorno no chamador
            if (tron)
            {
               printf(" %d: POPA\t{%s\n", PCantes, MH[CI[PCantes].src]);
               printf(" retorno de chamada para PC = %d\n", PC);
            }
            break;

         case 23: // LDAC var
            AC = VAR[CI[PC].pr1].val;
            if (tron)
            {
               printf(" %d: LDAC %d\t{%s\n", PC, VAR[CI[PC].pr1].val, MH[CI[PC].src]);
               printf(" AC == %d\n", AC);
            }
            break;

         case 24: // STAC var
            VAR[CI[PC].pr1].val = AC;
            if (tron)
            {
               printf(" %d: STAC %d %s\t{%s\n", PC, AC, MH[CI[PC].src]);
               printf(" %s == %d\n", VAR[CI[PC].pr1].nome, VAR[CI[PC].pr1].val);
            }
            break;

      }
      if (tron)
         if (getchar() == 'x') break;  // saida em caso de loop infinito do ci
      PC++;
      //if (loop())  perguntaUsuarioSePara(); guarda uma cadeia de PCs e checa se tem repetiçao de subcadeia
      //sera implementada uma forma limitada de detectar repetiçao curta. Isso nao faz parte do projeto.
   }
   printf("\nEnd of process.\n");
}


void helpshell()
{
   printf(" Line commands:\n\n");
   printf(" load  ask for a MH source file, translate to and generate ci code.\n");
   printf(" run   execute the intermediate code previously loadded.\n");
   printf(" in    get the values to the program variables.\n");
   printf(" vars  display values of the program variables.\n");
   printf(" mh    display the MH source program previously loadded.\n");
   printf(" ci    display the intermediate code already generated.\n");
   printf(" tron  toggle trace on/off to activate/deactivate the debugger.\n");
   printf(" lang  change the MHShell user insterface language.\n");
   printf(" color change the colors of text and background. \n");
   printf(" bye   exit from MHshell.\n");
   printf(" ?     show this help... :)\n");
}

void prompt()
{
   printf("\n>>> ");
}

int getprompt()
{
   // le comando em string e retorna codigo int
   // load     1
   // run      2
   // input    3
   // disv     4
   // dismh    5
   // disci    6
   // tron     7
   char strcmd[40];

   fflush(stdin); // esvazia o buffer da acao dos scanf´s para permitir um uso correto do gets abaixo
   gets(strcmd);
   //puts(strcmd);

   if (strcasecmp(strcmd,"?")==0)
      return 10;
   if (strcasecmp(strcmd,"load")==0)
      return 1;
   if (strcasecmp(strcmd,"run")==0)
      return 2;
   if (strcasecmp(strcmd,"in")==0)
      return 3;
   if (strcasecmp(strcmd,"vars")==0)
      return 4;
   if (strcasecmp(strcmd,"mh")==0)
      return 5;
   if (strcasecmp(strcmd,"ci")==0)
      return 6;
   if (strcasecmp(strcmd,"tron")==0)
      return 7;
   if (strcasecmp(strcmd,"lang")==0)
      return 8;
   if (strcasecmp(strcmd,"bye")==0)
      return 0;
   if (strcasecmp(strcmd,"color")==0)
      return 9;
   else
   	return 99;
}

void header()
{
   system("color 1F");     // fundo 1 (azul) e texto  F (branco)
                           //       0  preto          2  verde
   printf("   ======================================================\n");
   printf("   ===== MHShell v2.00 2019        IFF Macae Brazil =====\n");
   printf("   ===== For basic Theoretical Algorithmics Courses =====\n");
   printf("   =====            and Introduction to Programming =====\n");
   printf("   =====                  by Marcelo Fagundes Felix =====\n");
   printf("   ======================================================\n");
}

void apresentacao()
{
   printf("   This is MHShell :  a tiny  environment  for experiments in\n");
   printf("the basis  of  programming.  We  use  a  minimal  programming\n");
   printf("language called MH  - an acronymous to 'Maquina Hipotetica' -\n");
   printf("that  possess  JUST 4 commands:  increment  a variable (x++),\n");
   printf("reset a variable (x = 0), LOOP-END (to iterates a block  just\n");
   printf("a number of times indicated by a variable) and GOTO.\n\n");
   printf("   Why someone would do write programs in a so poor language?\n");
   printf("We invite  you to discover. MH programming  stimulates a kind\n");
   printf("of reasonning that allow investigate the basis of programming\n");
   printf("activity  in  an  essencial way  that indicates to a newbie a\n");
   printf("solid fundation to his first skills.\n\n");
   printf("    Enjoy yourself!\n\n");
   printf("                                 Your teacher, Marcelo.\n\n\n\n");
   printf("Type ? to help line commands...\n");
}

void shell()
{
   int cmdprompt = 1;

   printf("\n");
   apresentacao();
   // helpshell();   // orientacoes da 1a tela

   while(cmdprompt)
   {
      prompt();
      cmdprompt = getprompt();

      switch (cmdprompt)
      {
      	case 0:
      	   system("cls");
      		printf(" Bye! (MHShell v.2.00 2019)\n\n");
      		printf(" Press any key to return...");
      		if(getchar())
      		   break;
         case 1:  // load arqMH
            loadMH();
            break;
         case 2:  // run arq carregado. Se nao houver arquivo carregado da erro
            execCI();
            break;
         case 3:  // input de variaveis
            carregaVAR();
            break;
         case 4:  // display de variaveis
            displayVAR();
            break;
         case 5:  // display de codigo MH carregado
            displayMH();
            break;
         case 6:  // display do CI carregado
            displayCI();
            break;
         case 7:  // tron/troff
            if (tron)
               printf("Trace off");
            else
               printf("Trace on");
            tron = 1 - tron;
            break;
         case 10:
            helpshell();
            break;
         case 9:
            //color();
            color++;
            if (color>2) color = 0;
            switch (color)
            {
               case 0:
                  system("color 1F");
                  break;
               case 1:
                  system("color 02");
                  break;
               case 2:
                  system("color 0F");
                  break;
            }
            break;
         default:
            printf(" - Invalid command...\n");
            break;

      }
   }
}

int main()
{
   header();
   shell();
}

