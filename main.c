#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_INPUT 1024 // Maksimalus input-o simboliu sk.
#define MAX_ARGS 64 // Maksimalus argumentu sk.

// Vartotojo input-o analizes funkcija
void parseCommand(char *input, char **args){
    char *token = strtok(input," \t\n");    // Skaidom input-a pagal tarpus, whitespace-us ir t.t.
    int i = 0;
    while(token!=NULL && i<MAX_ARGS-1){     // Vykdomas ciklas kol isanalizuotas visas input-as
        args[i++] = token;                  // Kiekvienas token-as (komanda, jos argumentas ar operatorius) saugomas atskirai
        token = strtok(NULL," \t\n");       // Gaunam sekancia token-a
    }
    args[i] = NULL;                         // Terminuojam paskutini args elementa tam, kad execvp() sustotu
}

// Srautu nukreipimas/tvarkykle
void redirectionHandle(char **args) {
    for(int i=0; args[i]!=NULL; i++){  // Pasileidziam cikla apdoroti kiekviena turima argumenta/komanda/operatoriu is args masyvo
        int fd = -1;                    // Failo deskriptorius, reikalingas nukreipimui
        int appendMode = 0;             // Ar vyks "append" operatorius (>>), 0 - ne, 1 - taip

        // Keli if-ai patikrinti, ar tai bus numeruotas nukreipimas (pvz. 2>), ar paprastas (>, <, >>)
        if(isdigit(args[i][0]) && args[i][1] == '>'){         // Kai numeruotas nukreipimas:
            fd = args[i][0] - '0';              // Gaunam skaiciu is argumento (pvz. 2 is 2>)
            appendMode = (args[i][2] == '>');   // Tikrinam ar tai numeruotas "append" operatorius, 0 - ne, 1 - taip
            args[i] = NULL;                     // Terminuojam paskutini elementa
        }
        else if(strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0 || strcmp(args[i], "<") == 0){    // Kai paparastas nukreipimas:
            fd = (strcmp(args[i], "<") == 0) ? STDIN_FILENO : STDOUT_FILENO;    // Ziurim, ar mums reikia irasyti output-a i faila per stdout, ar gauti duomenis is failo per stdin, STDIN_FILENO - 0, STDOUT_FILENO - 1
            appendMode = strcmp(args[i], ">>") == 0;                            // Tikrinam, ar paprastas "append" operatorius >>
            args[i] = NULL; 
        }

        // Tikrinam, ar nustatytas koks nors tinkamas deskriptorius ir ar turime input/output faila kaip argumenta
        if(fd!=-1 && args[i+1]!=NULL){
            int flags = O_WRONLY | O_CREAT | (appendMode ? O_APPEND : O_TRUNC);   // Nustatom failo busenas/teises: O_WRONLY - write only, O_CREATE - sukuriam jei jau neegzistuoja, toliau priklausomai nuo appendMode (>>) arba O_APPEND - prirasom failo pabaigoje, arba O_TRUNC - trinam egzistuojamo failo turini ir rasom
            if(fd == STDIN_FILENO) flags = O_RDONLY;  // Jeigu deskriptorius = 0, t.y. skaitom is failo, tada nustatom busena/teise O_RDONLY - read only.

            int redirFd = open(args[i+1], flags, 0644); // Atidarom faila pavadinimu is args[i+1], su teisemis is flags ir 0644 - autorius gali skaityti+rasyti, kiti - tik skaityti
            if(redirFd<0){          // Tikrinam klaidas atidarant faila
                perror("open");     
                exit(1);            // Rasome klaida i stderr srauta ir uzdarome su kodu 1
            }
            if(dup2(redirFd, fd)<0){    // Tikrinam ar pavyko duplikuoti failo deskriptoriu, t.y. ar fd rodo I ta pati faila kaip redirFd, t.y. kuri mes atidareme 47 eiluteje
                perror("dup2");
                exit(1);                // Rasome klaida i stderr srauta ir uzdarome su kodu 1
            }
            close(redirFd); // Uzdarome faila, kuri atidareme su siuo deskriptoriu
            args[i+1] = NULL; // Terminuojam paskutini elementa
            i++;
        }
    }
}

// Funkcija, atsakinga uz naudotojo ivestu komandu vykdyma
void execCmd(char **args){
    pid_t pid = fork();                 // "Sukuriam"(duplikuojam) nauja procesa komandu vykdymui

    if(pid<0){                       // Jeigu nepavyko sukurti procesa - rasome klaida i stderr srauta ir uzdarome su kodu 1
        perror("fork");
        exit(1);
    }else if(pid==0){                // Jeigu procesas tinkamas (child process su id 0), o tai reikalinga norint paleisti pacia UNIX programa su execvp(), vykdome komandos vykdyma
        redirectionHandle(args);
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    }else{                           // Jeigu pries tai jau buvo paleistas kazkoks child process ir jis turi id != 0, laukiam kol jis baigsis.
        int status;
        waitpid(pid, &status, 0);
    }
}

int main(){
    // Input-o masyvas
    char input[MAX_INPUT];
    // Argumentu masyvas
    char *args[MAX_ARGS];

    // Pasileido shell-as. Vartotojui rodomas pranesimas.
    printf("StreamShell$ StreamShell started. Please input your commands.\n");
    while (1) {
        printf("StreamShell$ ");
        // Valomas buffer-is vartotojo input-ui
        fflush(stdout);

        // Jeigu input-as tuscias - tiesiog rodoma nauja eilute
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Valomas naujos eilutes simbolis input-o gale
        input[strcspn(input, "\n")] = 0;

        // Jeigu vartotojas ivede "exit" - stabdome cikla ir iseiname
        if (strcmp(input, "exit") == 0) {
            printf("StreamShell> Exiting...\n");
            break;
        }

        // Analizuojamas vartotojo input-as
        parseCommand(input, args);

        // Jei argumentai netusti/ju yra - vykdomos isanalizuotos komandos.
        if (args[0] != NULL) {
            execCmd(args);
        }
    }
    
    return 0;
}