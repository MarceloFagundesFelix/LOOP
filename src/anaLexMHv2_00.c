/***********************************************************************************
   Arquivo:    "anaLexMHv2_00.c"
   Descricao:  Funcao getTok para MH e outras funcoes de analise lexica para o shell
   Autor:      Marcelo Fagundes Felix
   Data:       25/05/2019
************************************************************************************

Acrescimos:

21/mar/19
reconhece Tok 98 '//'
reconhece Tok 11 '+='

25/mar/19
reconhece IN 75 e OUT 76

20/abr/19
reconhece tok 12 e 13 '--' e '-='

17/maio/19
reconhece tok 14 15 e 16 *= /= e ^= (operadores binarios contraidos)

aceita \t no meio das linhas 20/05

ABBR RET MAIN
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define LIM          150
#define TAMMAXTOKEN  20
#define TAMMAXWORD   40
#define TAMMAXNUM    9

// Tokens de MH pura
#define TOK_ID_VAR         1     // alpha.(alpha|digit)*
#define TOK_ID_ROT         2     // alpha.(alpha|digit)*
#define TOK_OP_INCREM      3     // '++'
#define TOK_OP_ATRIB       4     // '='
#define TOK_CTE_NUMER      5     // {1..9}.digit* - precisa limitar o valor
                                 // Em MHpura so 0 (zero), na Plus pode >0
#define TOK_SEPAR_ROT      6     // ':'
#define TOK_PAL_RES_NAT    7     // NAT
#define TOK_PAL_RES_LOOP   8     // LOOP
#define TOK_PAL_RES_END    9     // END
#define TOK_PAL_RES_GOTO   10    // GOTO

// Tokens de MH plus
#define TOK_OP_SOMA        11    // '+='
#define TOK_OP_DECREM      12    // '--'
#define TOK_OP_SUBTRA      13    // '-='
#define TOK_OP_MULTIP      14    // '*='
#define TOK_OP_DIVISA      15    // '/='
#define TOK_OP_POTENC      16    // '^='

#define TOK_REL_IGUALDAD   31    // '=='
#define TOK_REL_DIFERENCA  32    // '!='
#define TOK_REL_MENOR      33    // '<'
#define TOK_REL_MAIOR      34    // '>'
#define TOK_REL_MENIGUAL   35    // '<='
#define TOK_REL_MAIORIGUAL 36    // '>='

#define TOK_LOG_CONJUNCAO  37    // '&&'
#define TOK_LOG_DISJUNCAO  38    // '||'
#define TOK_LOG_INVERSAO   39    // '!'

#define TOK_PAL_RES_MAIN   70    // MAIN
#define TOK_PAL_RES_IN     75    // IN
#define TOK_PAL_RES_OUT    76    // OUT
#define TOK_PAL_RES_ABBR   78    // ABBR
#define TOK_PAL_RES_RET    79    // RET

#define TOK_ID_ABREV       77    // alpha.(alpha|digit)*

#define TOK_COMENTARIO     80    // '//'

#define TOK_ENDOFLINE      98    // '\n'
#define TOK_ENDOFSTR       99    // '\0'

#define TOK_INEXISTENTE    0     // ''
#define TOK_INDEFINIDO     -1    // qq token que nao seja da linguagem


int posStr;          // posicao atual da analise lexica de str
int erroLexico = 0;  // Controla fim da Analise Lexica/traducao

typedef struct
{
   int cod   ;             // codigos dos tokens
   char val[TAMMAXWORD];   // valor string do token
}  Ttok;                   // Tipo token

Ttok tokAnterior={-1,""};  // armazena token anterior ao atual token lido
                           // eh usado para identificar id-rotulo apos um "GOTO"


/*************************************************************************************
Testa se posicao eh final da string
*/
short fimStr(int i, char s[])
{
   return(i==strlen(s));
}

/*************************************************************************************
Sinaliza Erro na analise lexica ligando variavel global de controle
*/
void lexError(char msg[])
{
   printf("Erro lexico: %s\n", msg);
   erroLexico = 1;      // interrompe analise e traducao
}

/*************************************************************************************
Dada uma posicao inicial i em s, retorna a 1a pos depois de uma sequencia de brancos
*/
int pulaBrancos(int i, char s[])

{
   int n = strlen(s);
   while ((s[i] == ' ' || s[i] == '\t') && i < n) i++;   // pula brancos ou tabs
   return(i);
   // o chamador deve verificar se chegou ao fim testando a posição aqui retornada
}

/*************************************************************************************
 Funcoes de apoio para a funcao getTok
 getWord  : palavra-reservada, identificador-Var ou identificador-Rot
 getNum   : 0 | (1..9)(0..9)*
*/


/*************************************************************************************
Obtem uma palavra em s e a retorna em w avancando a posicao i
lembrando que w e passada por referencia e i tambem
*/
void getWord(int * i, char s[], char w[TAMMAXWORD])
{
   int j=0;
   w[0]='\0';
   //printf("getWord recebeu i %d s%s\n", *i, s);
   if (isalpha(s[*i]))
   {
      w[j++]=s[(*i)++];
      while (isalnum(s[*i]))
         w[j++]=s[(*i)++];
   }
   w[j]='\0';
   if (j>TAMMAXWORD) lexError("Palavra excede tamanho limite.");
   //printf("getWord produziu %s tam %d\n", w, strlen(w));
   // a posicao *i termina no 1o caracter fora da palavra (' ', = + 0 ou qq outro...)
}

/*************************************************************************************
Obtem uma palavra em s e a retorna em num para avancando a posicao i
lembrando que num e passada por referencia e i tambem
FALTA EVITAR NUM COM NUMERO DE ALGARISMOS EXCESSIVO !
*/
int getNum(int * i, char s[], char num[TAMMAXNUM])
{
   int j=0;
   if (isdigit(s[*i]) && (s[*i]!='0'))
   {
      num[j++]=s[(*i)++];
      while (isdigit(s[*i]))
         num[j++]=s[(*i)++];
   }
   num[j]='\0';
}

/*************************************************************************************
Classifica todos os tokens validos da linguagem MH retornando val e cod do token
avançando o indice i da string em analise
indice passado por referencia termina posicionado no 1o caracter após o token
*/
Ttok getTok(int * i, char s[])
{
   char stokW[TAMMAXWORD], stokN[TAMMAXNUM]; // maior num eh 999.999.999
   Ttok t;
   int pos;

   *i = pulaBrancos(*i, s);   // se tiver brancos avanca e retorna a posicao do 1o
                              // caracter nao branco da string

   if (isalpha(s[*i]))
   {
      getWord(i, s, stokW);   // i passada por referencia volta com a posicao avancada
                              // e stokW (vetor eh endereco) com a string encontrada
      strcpy(t.val, stokW);   // registra o valor encontrado por getWord

      if       (strcasecmp(stokW,"LOOP") == 0)  t.cod = TOK_PAL_RES_LOOP;
      else if  (strcasecmp(stokW,"END")  == 0)  t.cod = TOK_PAL_RES_END;
      else if  (strcasecmp(stokW,"GOTO") == 0)  t.cod = TOK_PAL_RES_GOTO;
      else if  (strcasecmp(stokW,"NAT")  == 0)  t.cod = TOK_PAL_RES_NAT;
      else if  (strcasecmp(stokW,"IN")   == 0)  t.cod = TOK_PAL_RES_IN;
      else if  (strcasecmp(stokW,"OUT")  == 0)  t.cod = TOK_PAL_RES_OUT;
      else if  (strcasecmp(stokW,"MAIN") == 0)  t.cod = TOK_PAL_RES_MAIN;
      else if  (strcasecmp(stokW,"ABBR") == 0)  t.cod = TOK_PAL_RES_ABBR;
      else if  (strcasecmp(stokW,"RET")  == 0)  t.cod = TOK_PAL_RES_RET;
      else  // resta saber se é id-var ou id-rotulo ou id-abrev
            // basta inspecionar se ha ':' apos a palavra obtida no getWord
            // ou se ha outra id depois da atual
      {
         pos = pulaBrancos(*i, s);  // nao avanca o indice i,
                                    // apenas retorna o indice do 1o char nao branco
         //se ha GOTO(cod 10) antes do token atual ou : depois dele, temos id-rotulo
         if ( (tokAnterior.cod == TOK_PAL_RES_GOTO) ||  (s[pos] == ':') )
            t.cod = TOK_ID_ROT;  // o token visto eh um id-rotulo
         else if (((tokAnterior.cod == TOK_PAL_RES_ABBR) && isalpha(s[pos])) || // ABBR antes com alpha depois
                  ((tokAnterior.cod == TOK_OP_ATRIB) && isalpha(s[pos]))) // Atribuicao antes e alpha depois
            t.cod = TOK_ID_ABREV;  // entao temos um token id-abrev
         else
            t.cod = TOK_ID_VAR;  // em ultimo caso o token visto eh um id-var
      }
      tokAnterior.cod = t.cod;
      return(t);
   } // aqui termina analise de tokens do tipo word (id-var, id-rot, id-abrev, pal-reserv)

   else  // se nao comeca com alpha, pode ser //, ++, +=, = ou numerico
         // ou ainda -- ou -=
         // ou *= (acrescentado em 17/05) /= ou ^=
   {
      if( s[*i] == '/')
      {
         (*i)++;
         if( s[*i] == '/' )    // eh marca de inicio de comentario
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "//"
            //t.cod = 80;   poderia ser 80, mas eh bem mais simples identifica-lo
            // com o tokem 98 (\n) de forma que // encerra a linha como o \n.
            t.cod = TOK_ENDOFLINE;  // cod de token comentario identico ao do '\n'
            strcpy(t.val,"//");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else if( s[*i] == '=' )  // achou um token '/='
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "/="
            t.cod = TOK_OP_DIVISA;  // token contraido de divisao
            strcpy(t.val,"/=");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else
         {
            (*i)++;  // avanca para considerar que o char colado na / foi lido
            t.cod = -1;    // token indefinido
            strcpy(t.val, "/?");
            lexError("Undefined token: '/' followed by invalid char.");
            tokAnterior.cod = t.cod;
            return(t);
         }
      }
      else if( s[*i] == '+' )
      {
         (*i)++;
         if( s[*i] == '+' )    // e tem + na posicao seguinte tambem
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "++"
            t.cod = TOK_OP_INCREM;  // token incremento 1
            strcpy(t.val,"++");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else if( s[*i] == '=' )    // e tem = na posicao seguinte tambem
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "+="
            t.cod = TOK_OP_SOMA;  // token incremento valor
            strcpy(t.val,"+=");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else
         {
            (*i)++;  // avanca para considerar que o char colado no + foi lido
            t.cod = TOK_INDEFINIDO;    // token indefinido
            strcpy(t.val, "+?");
            lexError("Undefined token: '+' followed by invalid char.");
            tokAnterior.cod = t.cod;
            return(t);
         }
      }
      else if( s[*i] == '-' ) // -- ou -= ou -?
      {
         (*i)++;
         if( s[*i] == '-' )    // e tem - na posicao seguinte tambem
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "--"
            t.cod = TOK_OP_DECREM; // token decremento
            strcpy(t.val,"--");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else if( s[*i] == '=' )    // e tem = na posicao seguinte tambem
         {
            (*i)++;     // avancou o indice para 1a posicao depois do "-="
            t.cod = TOK_OP_SUBTRA;  // token incremento valor
            strcpy(t.val,"-=");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else
         {
            (*i)++;  // avanca para considerar que o char colado no - foi lido
            t.cod = TOK_INDEFINIDO;    // token indefinido
            strcpy(t.val, "-?");
            lexError("Undefined token: '-' followed by invalid char.");
            tokAnterior.cod = t.cod;
            return(t);
         }
      }
      else if( s[*i] == '*' )
      {
         (*i)++;
         if( s[*i] == '=' )    // e tem = na posicao seguinte
         {
            (*i)++;     // avanca o indice para 1a posicao depois do "*="
            t.cod = TOK_OP_MULTIP;  // token multiplicacao contraida
            strcpy(t.val,"*=");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else
         {
            (*i)++;  // avanca para considerar que o char colado no * foi lido
            t.cod = TOK_INDEFINIDO;    // token indefinido
            strcpy(t.val, "*?");
            lexError("Undefined token: '*' followed by invalid char.");
            tokAnterior.cod = t.cod;
            return(t);
         }
      }
      else if( s[*i] == '^' )
      {
         (*i)++;
         if( s[*i] == '=' )    // e tem = na posicao seguinte
         {
            (*i)++;     // avanca o indice para 1a posicao depois do "^="
            t.cod = TOK_OP_POTENC;  // token potenciacao contraida
            strcpy(t.val,"^=");
            tokAnterior.cod = t.cod;
            return(t);
         }
         else
         {
            (*i)++;  // avanca para considerar que o char colado no ^ foi lido
            t.cod = TOK_INDEFINIDO;    // token indefinido
            strcpy(t.val, "^?");
            lexError("Undefined token: '^' followed by invalid char.");
            tokAnterior.cod = t.cod;
            return(t);
         }
      }

      else  // pode ser = : ou numerico

         if( s[*i] == '=' )
         {
            (*i)++;     // avanca indice para depois do token
            t.cod = TOK_OP_ATRIB;  // token atribuicao
            strcpy(t.val, "=");
            tokAnterior.cod = t.cod;
            return(t);
         }

         else  // pode ser ':'

            if( s[*i] == ':')
            {
               (*i)++;     // avanca indice para depois do token
               t.cod = TOK_SEPAR_ROT;  // token separador
               strcpy(t.val, ":");
               tokAnterior.cod = t.cod;
               return(t);
            }

            else // ou numerico ou ainda um token indefinido (token fora da linguagem)
            {
               if (isdigit(s[*i]))  // pode ser um token numerico
                  if ( s[*i] == '0')   // temos o numero zero
                  {
                     (*i)++;
                     t.cod = TOK_CTE_NUMER;     // token numerico
                     strcpy(t.val, "0");
                     tokAnterior.cod = t.cod;
                     return(t);
                  }
                  else  // eh um numero maior que zero
                  {
                     getNum(i,s,stokN); // getNum ja avanca o indice i passado por ref
                     t.cod = TOK_CTE_NUMER;
                     strcpy(t.val, stokN);
                     tokAnterior.cod = t.cod;
                     return(t);
                  }
               else

               if(s[*i]=='\0')   // pode ser marca de fim de string
               {
                  t.cod = TOK_ENDOFSTR;
                  strcpy(t.val, "EoStr \\0");
                  tokAnterior.cod = t.cod;
                  return(t);
               }

               else

               if(s[*i]=='\n')   // ou marca de fim de linha
               {
                  t.cod = TOK_ENDOFLINE;
                  strcpy(t.val, "EoL \\n");
                  tokAnterior.cod = t.cod;
                  return(t);
               }

               else // nao sendo nada disso, temos um token indefinido
               {
                  (*i)++;  // senao avancar trava
                  t.cod = TOK_INDEFINIDO;
                  strcpy(t.val, "???");
                  lexError("Token invalido.");
                  tokAnterior.cod = t.cod;
                  return(t);
               }
            }

   }
}
/*************************************************************************************
Imprime tipologia do token e posicao do mesmo na linha
*/
int printCategoriaTok(int p, Ttok t)
{
   printf("%d : ", p);
   switch (t.cod)
   {
      case -1: printf("token indefinido\n");
               break;
      case 0:  printf("token inexistente\n");
               break;
      case 1:  printf("id variavel: %s\n", t.val);
               break;
      case 2:  printf("id rotulo: %s\n", t.val);
               break;
      case 3:  printf("op incremento\n");
               break;
      case 4:  printf("op atribuicao\n");
               break;
      case 5:  printf("numero: %s\n", t.val);
               break;
      case 6:  printf("separador de rotulo\n");
               break;
      case 7:  printf("NAT palavra reservada %s\n", t.val);
               break;
      case 8:  printf("LOOP palavra reservada %s\n", t.val);
               break;
      case 9:  printf("END palavra reservada %s\n", t.val);
               break;
      case 10: printf("GOTO palavra reservada %s\n", t.val);
               break;
      case 98: printf("Quebra de linha: %s\n", t.val);   // '\n'
               break;
      case 99: printf("Fim de string: %s\n", t.val);     // '\0'
               break;
   }
}


/*
int main()
{
   FILE *arq;
   char mh[200][LIM]; //puts nao funciona com char *s;
   int count;
   int pos;
   Ttok tok;

   tokAnterior.cod = -1;   // Usado para determinar "GOTO id-rotulo"

   arq = fopen("teste.mh", "r");
   if (arq == NULL)
   {
      printf("Erro ao abrir o arquivo!\n");
      return(1);
   }
   else
   {
      count=1; // contador de linhas do arquivo fonte
      while(fgets(mh[count], LIM, arq)) //Carrega cada linha
      {
         printf("\nLinha %d (%d): %s\n", count, strlen(mh[count]), mh[count]);
         pos = 0; // indice indicando inicio da string que será passado por referencia
         do
         {
            tok = getTok(&pos, mh[count]);
            printCategoriaTok(pos, tok);
         }
         while ((tok.cod!=99) && (tok.cod!=98)); // fim de linha
         count++;
      }
      printf("\n Fim da analise arquivo fonte MH.\n");
   }
}
*/
