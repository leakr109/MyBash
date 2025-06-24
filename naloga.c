#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h> 
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_TOKENS 50
#define MAX_SIZE 100

char line[MAX_SIZE];
char line_original[MAX_SIZE];
char* tokens[MAX_TOKENS];
int token_count;

char inputR[MAX_SIZE];
char outputR[MAX_SIZE];
int background = 0;

int debug_level = 0;
char prompt[8];
int status = 0;
int exit_status = false;

char procfs[50];

//------------------------------------------------------------------------------------

int izpis(){
    printf("Input line: '%s'\n", line_original);
    for(int i = 0; i < token_count; i++){
        printf("Token %d: '%s'\n", i, tokens[i]);
    }
    return 0;
}

//vrstico razcleni na posamezne simbole
int tokenize(){
    token_count = 0;
    int len = strlen(line);
    for(int i = 0; i < len; i++){
        if(line[i] == '\n'){
            line[i] = '\0';
        }
    }
    strcpy(line_original, line);

    bool najden = false;
    int ix = 0;
    char znak = line[ix];
    while(znak != '\0'){
        if(znak == ' '){
            line[ix] = '\0';
            najden = false;
        }else{
            if(!najden){
                if(znak == '"'){
                    line[ix] = '\0';
                    ix++;
                    if(line[ix] != '"'){
                        tokens[token_count++] = line + ix;
                    }
                    while(ix < len && line[ix] != '"'){
                        ix++;
                    }
                    line[ix] = '\0';
                }else if(znak == '#'){
                    break;
                }
                else{
                    najden = true;
                    tokens[token_count++] = line + ix;
                }
            }
        }
        ix++;
        znak = line[ix];
    }
    return token_count;
}

//preusmeritev vhoda, preusmeritev izhoda ali izvajanje v ozadju
int parse(){
    tokens[token_count] = NULL;
    int st = 0;
    for(int i = 3; i > 0; i--){
        if((token_count - i) >= 0){
            char znak = tokens[token_count - i][0];
            char naslednji = tokens[token_count - i][1];
            if(znak == '<' && naslednji != '\0'){
                strcpy(inputR, tokens[token_count - i] + 1);
                if(debug_level > 0) printf("Input redirect: '%s'\n", inputR); 
                tokens[token_count - i] = NULL;
                st++;    
                int vhod = open(inputR, O_RDONLY);
                fflush(stdout);
                dup2(vhod, STDIN_FILENO);
                close(vhod);
            }else if(znak == '>' && naslednji != '\0'){
                strcpy(outputR, tokens[token_count - i] + 1);
                if(debug_level > 0) printf("Output redirect: '%s'\n", outputR); 
                tokens[token_count - i] = NULL;
                st++; 
                int izhod = open(outputR, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                fflush(stdout);
                dup2(izhod, STDOUT_FILENO);
                close(izhod);
            }else if(znak == '&' && naslednji == '\0'){
                background = 1;
                if(debug_level > 0) printf("Background: %d\n", background); 
                tokens[token_count - i] = NULL;
                st++; 
            }
            fflush(stdout);
        }
    }
    token_count -= st;
    return 0;
}

//------------------------------------------------------------------------------------
//BUILTIN
char* ukazi_builtin[] = {"debug", "prompt", "status", "exit", "help", 
                        "print", "echo", "len", "sum", "calc", "basename", "dirname",
                        "dirch", "dirwd", "dirmk", "dirrm", "dirls", 
                        "rename", "unlink", "remove", "linkhard", "linksoft", "linkread", "linklist", "cpcat",
                        "pid", "ppid", "uid", "euid", "gid", "egid", "sysinfo",
                        "proc", "pids", "pinfo",
                        "waitone", "waitall",
                        "pipes"};
int N = 38;

int debug_builtin(){
    if(token_count > 1){
        debug_level = atoi(tokens[1]);
    }else{
        printf("%d\n", debug_level);
    }
    return 0;
}

int prompt_builtin(){
    if(token_count > 1){
        if(strlen(tokens[1]) > 8){
            return 1;
        }
        strcpy(prompt, tokens[1]);
    }else{
        printf("%s\n", prompt);
    }
    return 0;
}

int status_builtin(){
    printf("%d\n", status);
    fflush(stdout);
    return status;
}

int exit_builtin(){
    if(token_count > 1){
        status = atoi(tokens[1]);
    }
    exit_status = true;
    if(debug_level > 0) printf("Exit status: %d\n", status);
    fflush(stdout);
    exit(status);
    return 0;
}

int help_builtin(){
    printf("Podprti vgrajeni ukazi:");
    for(int i = 0; i < N; i++){
        printf(" %s", ukazi_builtin[i]);
    }
    printf("\n");
    return 0;
}

int print_builtin(){
    for(int i = 1; i < token_count; i++){
        printf("%s", tokens[i]);
        if(i < token_count - 1){
            printf(" ");
        }
    }
    return 0;
}

int echo_builtin(){
    for(int i = 1; i < token_count; i++){
        printf("%s", tokens[i]);
        if(i < token_count - 1){
            printf(" ");
        }
    }
    printf("\n");
    fflush(stdout);
    return 0;
}

int len_builtin(){
    int len = 0;
    for(int i = 1; i < token_count; i++){
        len += strlen(tokens[i]);
    }
    printf("%d\n", len);
    return 0;
}

int sum_builtin(){
    int sum = 0;
    for(int i = 1; i < token_count; i++){
        sum += atoi(tokens[i]);
    }
    printf("%d\n", sum);
    return 0;
}

int calc_builtin(){
    int rezultat = 0;
    switch (tokens[2][0]){
    case '+': rezultat = atoi(tokens[1]) + atoi(tokens[3]); break;
    case '-': rezultat = atoi(tokens[1]) - atoi(tokens[3]); break;
    case '*': rezultat = atoi(tokens[1]) * atoi(tokens[3]); break;
    case '/': rezultat = atoi(tokens[1]) / atoi(tokens[3]); break;
    case '%': rezultat = atoi(tokens[1]) % atoi(tokens[3]); break;
    }
    printf("%d\n", rezultat);
    return 0;
}

int basename_builtin(){
    if(token_count == 1){
        return 1;
    }
    int len = strlen(tokens[1]);
    int odmik = 0;
    for(int i = len - 1; i >= 0; i--){
        if(tokens[1][i] == '/'){
            odmik = i;
            break;
        }
    }
    printf("%s\n", tokens[1] + odmik + 1);
    return 0;
}

int dirname_builtin(){
    if(token_count == 1){
        return 1;
    }
    int len = strlen(tokens[1]);
    int odmik = 0;
    for(int i = len - 1; i >= 0; i--){
        if(tokens[1][i] == '/'){
            odmik = i;
            break;
        }
    }
    char* dname = malloc(odmik);
    strncpy(dname, tokens[1], odmik);
    printf("%s\n", dname);
    return 0;
}

//delo z imeniki
int dirch_builtin(){    //zamenjava imenika
    if(token_count > 1){
        if(chdir(tokens[1]) < 0){
            int koda = errno;
            perror("dirch");
            fflush(stderr);
            return koda;
        }
    }else{
        chdir("/");
    }
    return 0;
}

int dirwd_builtin(){
    char pot[MAX_SIZE];
    getcwd(pot, MAX_SIZE);
    int len = strlen(pot);

    if(len == 1 || (token_count > 1 && strcmp(tokens[1], "full") == 0)){
        printf("%s\n",pot);
    }else{ 
        int odmik = 0;
        for(int i = len - 1; i >= 0; i--){
            if(pot[i] == '/'){
                odmik = i;
                break;
            }
        }
        printf("%s\n", pot + odmik + 1);
    }
    fflush(stdout);
    return 0;
}

int dirmk_builtin(){
    if(mkdir(tokens[1], 0755) < 0){
        int koda = errno;
        perror("dirmk");
        fflush(stderr);
        return koda;
    }
    return 0;
}

int dirrm_builtin(){
    if(rmdir(tokens[1]) < 0){
        int koda = errno;
        perror("dirrm");
        fflush(stderr);
        return koda;
    }
    return 0;
}

int dirls_builtin(){
    DIR* dir;
    if(token_count > 1){
        dir = opendir(tokens[1]);
    }else{
        dir = opendir(".");
    }
    struct dirent * entry;
    while((entry = readdir(dir)) != 0) {
        printf("%s  ", entry->d_name);
    }
    printf("\n");
    fflush(stdout);
    closedir(dir);
    return 0;
}

//delo z datotekami
int rename_builtin(){
    rename(tokens[1], tokens[2]);
    return 0;
}

int unlink_builtin(){
    unlink(tokens[1]);
    return 0;
}

int remove_builtin(){
    remove(tokens[1]);
    return 0;
}

int linkhard_builtin(){
    link(tokens[1], tokens[2]);
    return 0;
}

int linksoft_builtin(){
    symlink(tokens[1], tokens[2]);
    return 0;
}

int linkread_builtin(){
    char cilj[MAX_SIZE];
    int len = readlink(tokens[1], cilj, MAX_SIZE);
    cilj[len] = '\0';
    printf("%s\n", cilj);
    fflush(stdout);
    return 0;
}

int linklist_builtin(){
    struct stat cilj;
    stat(tokens[1], &cilj);

    DIR* dir = opendir(".");
    struct dirent * entry;
    while((entry = readdir(dir)) != 0) {
        struct stat dat;
        stat(entry->d_name, &dat);  //pridobi podatke o datoteki

        if(cilj.st_ino == dat.st_ino){
            printf("%s  ", entry->d_name);
        }        
    }
    printf("\n");
    fflush(stdout);
    closedir(dir);
    return 0;
}

int napaka(){
    int koda = errno;
    perror(tokens[0]);
    fflush(stderr);
    return koda;
}

int cpcat_builtin(){
    int vhod = 0; 
    int izhod = 0;

    if(token_count >= 3){
        if(strcmp(tokens[1], "-") == 0){         // kopiranje stdin v b.txt
            vhod = STDIN_FILENO;
            if(vhod < 0) return napaka();
            izhod = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(izhod < 0) return napaka();
            
        }else{                                   //kopiranje iz a.txt v b.txt
            vhod = open(tokens[1], O_RDONLY);
            if(vhod < 0) return napaka();
            izhod = open(tokens[2], O_WRONLY | O_CREAT, 0644);
            if(izhod < 0) return napaka();
        }
    }else if(token_count == 2){                  //izpis datoteke a.txt
        vhod = open(tokens[1], O_RDONLY);
        if(vhod < 0) return napaka();
        izhod = STDOUT_FILENO;
    }else{                                       //kopiranje stdin v stdout
        vhod = STDIN_FILENO;
        izhod = STDOUT_FILENO;
    }

    char buf[4096];
    int n = read(vhod, buf, sizeof(buf));
    if(n < 0){
        return napaka();
    }else{
        if(write(izhod, buf, n) < 0) return napaka();
    }
    if (vhod != STDIN_FILENO) close(vhod);
    if (izhod != STDOUT_FILENO) close(izhod);
    return 0;
}

//procesi
int pid_builtin(){
    printf("%d\n", getpid());
    return 0;
}

int ppid_builtin(){
    printf("%d\n", getppid());
    return 0;
}

int uid_builtin(){
    printf("%d\n", getuid());
    return 0;
}

int euid_builtin(){
    printf("%d\n", geteuid());
    return 0;
}

int gid_builtin(){
    printf("%d\n", getgid());
    return 0;
}

int egid_builtin(){
    printf("%d\n", getegid());
    return 0;
}

int sysinfo_builtin(){
    struct utsname sistem;
    uname(&sistem);
    printf("Sysname: %s\n", sistem.sysname);
    printf("Nodename: %s\n", sistem.nodename);
    printf("Release: %s\n", sistem.release);
    printf("Version: %s\n", sistem.version);
    printf("Machine: %s\n", sistem.machine);
    return 0;
}

int proc_builtin(){     //nastavi pot do procfs
    if(token_count == 1){
        printf("%s\n", procfs);
        fflush(stdout);
    }else{
        if(access(tokens[1], F_OK|R_OK) == 0){
            strcpy(procfs, tokens[1]);
        }else{
            return 1;
        }
    }
    return 0;
}

int primerjaj(const void* a, const void* b) {
    return (*(int*)a - *(int*)b); // naraščajoče
}

int* getPids(){
    DIR* dir = opendir(procfs);
    int* pids = calloc(MAX_SIZE, sizeof(int));
    int ix = 0;

    struct dirent * entry;
    while((entry = readdir(dir)) != 0) {
        char* ime = entry->d_name;
        int len = strlen(ime);
        bool jeStevilo = true;
        for(int i = 0; i < len; i++){
            if(!isdigit(ime[i])){
                jeStevilo = false;
                break;
            }
        }
        if(jeStevilo){
            pids[ix] = atoi(ime);
            ix++;
        }
    }
    qsort(pids, ix, sizeof(int), primerjaj);
    closedir(dir);
    return pids;
}

int pids_builtin(){     // izpise PID-e trenutnih procesov
    int* pids = getPids();
    for(int i = 0; pids[i] != 0; i++){
        printf("%d\n", pids[i]);
    }
    fflush(stdout);
    return 0;
}

int pinfo_builtin(){    //izpise podatke za vsak PID
    int* pids = getPids();
    char pot[MAX_SIZE];
    char buf[4096];
    int stat = 0;
    printf("%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");

    for(int i = 0; pids[i] != 0; i++){
        snprintf(pot, MAX_SIZE, "%s/%d/stat", procfs, pids[i]);
        stat = open(pot, O_RDONLY);
        read(stat, buf, sizeof(buf));
        
        //pridobi podatke: pid (ime) stanje ppid
        int odmik = 0;
        int len = 0;
        while(buf[odmik] != '('){
            odmik++;
        }
        odmik++;
        while(buf[odmik + len] != ' '){
            len++;
        }
        char ime[len];
        strncpy(ime, buf + odmik, len - 1);
        ime[len - 1] = '\0';

        odmik += len + 1;
        char stanje = buf[odmik];

        odmik += 2;
        int ppid = 0;
        while(buf[odmik] != ' '){
            ppid *= 10;
            ppid += buf[odmik] - '0';
            odmik++;
        }

        printf("%5d %5d %6c %s\n", pids[i], ppid, stanje, ime);
        close(stat);
    }
    fflush(stdout);
    return 0;
}

//signali
void sigchld_handler (int signum){ //handler se izvede ko kakšen otrok pošlje sigchld signal, kar prepreči zombie procese
  int pid, stat, serrno;
  serrno = errno;
  while (1)
    {
      pid = waitpid (WAIT_ANY, &stat, WNOHANG);  //pobere vse KONCANE procese
      if (pid < 0){  //napaka
          //perror ("waitpid");
          break;
        }
      if (pid == 0) break; //ni nobenega koncanega procesa
    }
  errno = serrno;
}

int waitone_builtin(){
    int stat = 0;
    if(token_count  == 2){
        waitpid(atoi(tokens[1]), &stat, 0);
    }else{
        wait(&stat);
    }
    fflush(stdout);
    if(WEXITSTATUS(stat)) return WEXITSTATUS(stat);
    return 0;
}

int waitall_builtin(){
    int stat;
    while(wait(&stat) > 0)  // POCAKA vse izvajajoce procese
    fflush(stdout);
    return 0;
}


// cevovod
int find_builtin();
int pipes_builtin() {
    //printf("tc: %d\n",token_count);
    int n = token_count - 1;
    int fd[2];
    int prevIN = STDIN_FILENO;

    //shrani tokens in ga resetiraj
    char* newTokens[MAX_TOKENS];
    for(int i = 0; i < MAX_TOKENS; i++){
        if(i < token_count){
            newTokens[i] = tokens[i];
        }
        tokens[i] = NULL;
    }

    for(int i = 0; i < n; i++){
        if(i < n - 1){
            pipe(fd);
        }
        fflush(stdin);
        if(!fork()){
            //printf("i: %d\n", i);
            //preusmeri vhod
            if(i != 0){
                dup2(prevIN, STDIN_FILENO);
                close(prevIN);
            }
            //preusmeri izhod
            if(i < n - 1){
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]); close(fd[1]);
            }
            
            strcpy(line, newTokens[i+1]);
            token_count = 0;
            tokenize();
            //printf("%s\n", tokens[0]);
            fflush(stdout);
            int stat = find_builtin();
            exit(stat);
        }
        if(i != 0) close(prevIN);
        if(i < n - 1){
            close(fd[1]);
            prevIN = fd[0];
        }
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    return 0;
}

int (*funkcije_builtin[])() = {debug_builtin, prompt_builtin, status_builtin, exit_builtin, help_builtin, 
                                print_builtin, echo_builtin, len_builtin, sum_builtin, calc_builtin, basename_builtin, dirname_builtin,
                                dirch_builtin, dirwd_builtin, dirmk_builtin, dirrm_builtin, dirls_builtin,
                                rename_builtin, unlink_builtin, remove_builtin, linkhard_builtin, linksoft_builtin, linkread_builtin, linklist_builtin, cpcat_builtin,
                                pid_builtin, ppid_builtin, uid_builtin, euid_builtin, gid_builtin, egid_builtin, sysinfo_builtin,
                                proc_builtin, pids_builtin, pinfo_builtin,
                                waitone_builtin, waitall_builtin,
                                pipes_builtin};
                                
//pokliče vstrezno funkcijo za vgrajeni ukaz
int execute_builtin(int ix){
    char* ukaz = tokens[0];
    int stat = 0;
    if(background == 0){
        if(debug_level > 0) printf("Executing builtin '%s' in foreground\n", ukaz);  
        stat =  funkcije_builtin[ix]();
    }else{
        //izvajanje v ozadju
        fflush(stdin);
        fflush(stdout);
        if(debug_level > 0) printf("Executing builtin '%s' in background\n", ukaz);
        int pid = fork();
        if(pid == 0){
            int stat = funkcije_builtin[ix]();
            fflush(stdout);
            exit(stat);
        }
    }
    return stat;
}

//------------------------------------------------------------------------------------

//zažene zunanji ukaz
int execute_external(){
    if(debug_level > 0){
        printf("External command '");
        for(int i = 0; i < token_count; i++){
            printf("%s", tokens[i]);
            if(i < token_count - 1){
                printf(" ");
            }
        }
        printf("'\n");
    }

    fflush(stdin);
    fflush(stdout);
    int pid = fork();
	if (pid == 0) {
		execvp(tokens[0], tokens); //izvede zunanji ukaz
        perror("exec");
		exit(127);
	}else if (pid > 0) {
        if(background == 0){ //izvajanje v ospredju
            int stat;
            waitpid(pid, &stat, 0);
            return WEXITSTATUS(stat);
        }
	}else{
		int koda = errno;
        perror("fork");
        return koda;
	}
    fflush(stdout);
    return 0;
}

//------------------------------------------------------------------------------------

//poišče ali obstaja vgrajena funkcija, drugače požene zunanjo
int find_builtin(){
    if(token_count == 0) return status;
    for(int i = 0; i < N; i++){
        if(strcmp(tokens[0], ukazi_builtin[i]) == 0){
            return execute_builtin(i);
        }
    }
    return execute_external();
}

//------------------------------------------------------------------------------------

int main(){
    signal(SIGCHLD, sigchld_handler);
    //nastavimo na trenutni imenik
    char start[MAX_SIZE];
    getcwd(start, MAX_SIZE);
    chdir(start);

    strcpy(prompt, "mysh");
    strcpy(procfs, "/proc");

    while(true){
        fflush(stdout);
        background = 0;
        strcpy(inputR, ""); strcpy(outputR, "");
        int vhod = dup(0);
        int izhod = dup(1);

        if(isatty(STDIN_FILENO)) printf("%s> ", prompt);
        if(fgets(line, MAX_SIZE, stdin) == NULL){
            if(status > 0) exit_builtin();
            break; 
        }
        tokenize();
        if(debug_level > 0){
            izpis();
        }
        parse();    //nastavimo vhod/izhod
        status = find_builtin();
        fflush(stdout);

        dup2(vhod, 0);
        dup2(izhod, 1);
        if(exit_status) break;
    }
 
    return status;
}