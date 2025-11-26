#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NAME 64
#define MAX_CHILDREN 32
#define MAX_INPUT 256

// Simulação bem simples de "disco"
#define DISK_SIZE   8192        // bytes totais
#define BLOCK_SIZE  64          // bytes por bloco
#define NUM_BLOCKS  (DISK_SIZE / BLOCK_SIZE)

typedef struct Directory Directory;
typedef struct File File;

typedef enum {
    FILE_TEXT = 0,
    FILE_BINARY = 1,
    FILE_NUMERIC = 2,
    FILE_PROGRAM = 3
} FileType;

// Permissões no estilo chmod numérico (ex.: 750, 644)
typedef struct {
    int mode;   // 0..777 (ex.: 644, 755)
} Permissions;

// Simulação de usuário atual
typedef enum {
    USER_OWNER = 0,
    USER_GROUP = 1,
    USER_OTHERS = 2
} UserClass;

struct File {
    char name[MAX_NAME];
    FileType type;
    int size;               // em bytes
    int id;                 // "inode" simulado
    Permissions perms;
    time_t created_at;
    time_t modified_at;
    time_t accessed_at;
    Directory *parent;

    // Conteúdo em memória (pra facilitar debug/comparar com Linux)
    char *content;

    // Simulação de blocos (3.4)
    int blocks[16];         // índices de blocos alocados no "disco"
    int block_count;
};

struct Directory {
    char name[MAX_NAME];
    Directory *parent;
    Directory *subdirs[MAX_CHILDREN];
    int subdir_count;
    File *files[MAX_CHILDREN];
    int file_count;
};

// Globais
Directory *rootDir = NULL;
Directory *currentDir = NULL;
int next_inode_id = 1;

// "Disco" simulado
char DISK[DISK_SIZE];
int BLOCK_FREE[NUM_BLOCKS]; // 0 = livre, 1 = ocupado

// Usuário atual (pra simplificar vamos assumir sempre OWNER)
UserClass currentUser = USER_OWNER;

// ====================== DISCO / BLOCOS ======================

void init_disk() {
    memset(DISK, 0, sizeof(DISK));
    for (int i = 0; i < NUM_BLOCKS; i++) {
        BLOCK_FREE[i] = 0;
    }
}

// Encontra um bloco livre - versão bem simples
int allocate_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (BLOCK_FREE[i] == 0) {
            BLOCK_FREE[i] = 1;
            return i;
        }
    }
    return -1; // sem espaço
}

void free_block(int block_index) {
    if (block_index >= 0 && block_index < NUM_BLOCKS) {
        BLOCK_FREE[block_index] = 0;
    }
}

int calc_blocks_needed(int size) {
    if (size <= 0) return 0;
    return (size + BLOCK_SIZE - 1) / BLOCK_SIZE; // arredonda pra cima
}

void free_file_blocks(File *f) {
    for (int i = 0; i < f->block_count; i++) {
        int b = f->blocks[i];
        if (b >= 0) {
            free_block(b);
            f->blocks[i] = -1;
        }
    }
    f->block_count = 0;
}

// grava o conteúdo do arquivo em blocos do "disco" DISK[]
void store_file_in_blocks(File *f, const char *data) {
    int size = f->size;

    // 1) libera blocos antigos
    free_file_blocks(f);

    if (size <= 0 || data == NULL) {
        return;
    }

    int needed = calc_blocks_needed(size);

    if (needed > 16) {
        printf("Arquivo muito grande para esta simulacao (max 16 blocos).\n");
        needed = 16;
        size = needed * BLOCK_SIZE; // limita bruto
    }

    f->block_count = 0;
    int offset = 0;

    for (int i = 0; i < needed; i++) {
        int b = allocate_block();
        if (b == -1) {
            printf("Sem espaco em disco para alocar blocos.\n");
            break;
        }

        f->blocks[f->block_count++] = b;

        int remaining = size - offset;
        int bytes_this_block = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        // copia do buffer 'data' para o bloco no DISK
        memcpy(&DISK[b * BLOCK_SIZE], data + offset, bytes_this_block);

        // zera o resto do bloco, se sobrar espaço
        if (bytes_this_block < BLOCK_SIZE) {
            memset(&DISK[b * BLOCK_SIZE + bytes_this_block], 0,
                   BLOCK_SIZE - bytes_this_block);
        }

        offset += bytes_this_block;
    }
}

// reconstrói o conteúdo do arquivo lendo os blocos do DISK
char *read_file_from_blocks(File *f) {
    if (f->block_count == 0 || f->size <= 0) {
        char *empty = (char *) malloc(1);
        if (!empty) {
            perror("malloc");
            exit(1);
        }
        empty[0] = '\0';
        return empty;
    }

    char *buffer = (char *) malloc(f->size + 1);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    int offset = 0;

    for (int i = 0; i < f->block_count && offset < f->size; i++) {
        int b = f->blocks[i];
        if (b < 0) continue;

        int remaining = f->size - offset;
        int bytes_this_block = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        memcpy(buffer + offset, &DISK[b * BLOCK_SIZE], bytes_this_block);
        offset += bytes_this_block;
    }

    buffer[f->size] = '\0';
    return buffer;
}

// ====================== UTILS ======================

// Converte modo numérico (ex.: 750) para string "rwxr-x---"
void mode_to_string(int mode, char *out) {
    int o = (mode / 100) % 10;
    int g = (mode / 10) % 10;
    int t = (mode % 10);

    int perms[3] = {o, g, t};
    int idx = 0;

    for (int i = 0; i < 3; i++) {
        int p = perms[i];
        out[idx++] = (p & 4) ? 'r' : '-';
        out[idx++] = (p & 2) ? 'w' : '-';
        out[idx++] = (p & 1) ? 'x' : '-';
    }
    out[idx] = '\0';
}

int has_permission(Permissions perms, UserClass who, char what) {
    int mode = perms.mode;
    int digit;
    if (who == USER_OWNER) digit = (mode / 100) % 10;
    else if (who == USER_GROUP) digit = (mode / 10) % 10;
    else digit = mode % 10;

    int bit = 0;
    if (what == 'r') bit = 4;
    else if (what == 'w') bit = 2;
    else if (what == 'x') bit = 1;

    return (digit & bit) != 0;
}

Directory *create_directory(const char *name, Directory *parent) {
    Directory *dir = (Directory *) malloc(sizeof(Directory));
    if (!dir) {
        perror("malloc");
        exit(1);
    }
    strncpy(dir->name, name, MAX_NAME);
    dir->name[MAX_NAME - 1] = '\0';
    dir->parent = parent;
    dir->subdir_count = 0;
    dir->file_count = 0;
    return dir;
}

File *create_file(const char *name, Directory *parent, FileType type, int mode) {
    File *f = (File *) malloc(sizeof(File));
    if (!f) {
        perror("malloc");
        exit(1);
    }
    strncpy(f->name, name, MAX_NAME);
    f->name[MAX_NAME - 1] = '\0';
    f->type = type;
    f->size = 0;
    f->id = next_inode_id++;
    f->perms.mode = mode;
    f->parent = parent;
    f->content = NULL;
    f->block_count = 0;
    for (int i = 0; i < 16; i++) {
        f->blocks[i] = -1;
    }

    time(&f->created_at);
    f->modified_at = f->created_at;
    f->accessed_at = f->created_at;

    return f;
}

void init_fs() {
    init_disk();
    rootDir = create_directory("/", NULL);
    currentDir = rootDir;
}

// ====================== DIR HELPERS ======================

Directory *find_subdir(Directory *dir, const char *name) {
    for (int i = 0; i < dir->subdir_count; i++) {
        if (strcmp(dir->subdirs[i]->name, name) == 0)
            return dir->subdirs[i];
    }
    return NULL;
}

File *find_file(Directory *dir, const char *name) {
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, name) == 0)
            return dir->files[i];
    }
    return NULL;
}

void print_path_rec(Directory *dir) {
    if (dir->parent != NULL) {
        print_path_rec(dir->parent);
        if (strcmp(dir->name, "/") != 0)
            printf("/%s", dir->name);
    } else {
        printf("/");
    }
}

void print_prompt() {
    print_path_rec(currentDir);
    printf(" > ");
}

// ====================== COMANDOS ======================

// mkdir <nome>
void cmd_mkdir(char *name) {
    if (currentDir->subdir_count >= MAX_CHILDREN) {
        printf("Erro: limite de subdiretorios atingido.\n");
        return;
    }
    if (find_subdir(currentDir, name) != NULL) {
        printf("Erro: diretorio ja existe.\n");
        return;
    }
    Directory *d = create_directory(name, currentDir);
    currentDir->subdirs[currentDir->subdir_count++] = d;
}

// ls
void cmd_ls() {
    // Diretorios
    for (int i = 0; i < currentDir->subdir_count; i++) {
        printf("[D] %s\n", currentDir->subdirs[i]->name);
    }
    // Arquivos
    for (int i = 0; i < currentDir->file_count; i++) {
        File *f = currentDir->files[i];
        char perms_str[10];
        mode_to_string(f->perms.mode, perms_str);
        printf("[F] %s  (id=%d, %d bytes, perms=%s)\n",
               f->name, f->id, f->size, perms_str);
    }
}

// cd <nome> ou cd .. ou cd /
void cmd_cd(char *arg) {
    if (strcmp(arg, "/") == 0) {
        currentDir = rootDir;
        return;
    }
    if (strcmp(arg, "..") == 0) {
        if (currentDir->parent != NULL)
            currentDir = currentDir->parent;
        return;
    }

    Directory *d = find_subdir(currentDir, arg);
    if (!d) {
        printf("Erro: diretorio nao encontrado.\n");
        return;
    }
    currentDir = d;
}

// touch <nome>
void cmd_touch(char *name) {
    File *f = find_file(currentDir, name);
    if (f) {
        time(&f->modified_at);
        return;
    }
    if (currentDir->file_count >= MAX_CHILDREN) {
        printf("Erro: limite de arquivos atingido.\n");
        return;
    }
    // padrão: arquivo texto com permissão 644
    f = create_file(name, currentDir, FILE_TEXT, 644);
    currentDir->files[currentDir->file_count++] = f;
}

// cat <nome>
void cmd_cat(char *name) {
    File *f = find_file(currentDir, name);
    if (!f) {
        printf("Erro: arquivo nao encontrado.\n");
        return;
    }
    if (!has_permission(f->perms, currentUser, 'r')) {
        printf("Permissao negada para leitura.\n");
        return;
    }
    char *data = NULL;

    if (f->block_count > 0) {
        data = read_file_from_blocks(f);
    } else if (f->content) {
        data = strdup(f->content);
    } else {
        data = strdup("");
    }

    printf("%s\n", data);
    free(data);

    time(&f->accessed_at);
}

// echo TEXTO > arquivo
// Aqui vamos simplificar: a linha inteira depois de "echo " até " > " é o texto.
void cmd_echo_redirect(char *line) {
    // Exemplo: echo oi mundo > teste.txt
    char *p = strstr(line, "echo ");
    if (!p) return;
    p += 5; // pula "echo "

    char *redir = strstr(p, " > ");
    if (!redir) {
        printf("Sintaxe invalida. Use: echo TEXTO > arquivo\n");
        return;
    }

    *redir = '\0';
    redir += 3; // agora redir aponta para nome do arquivo

    char *texto = p;
    char *nome_arquivo = redir;

    // tira \n do final do nome do arquivo
    nome_arquivo[strcspn(nome_arquivo, "\n")] = '\0';

    // cria arquivo se nao existir
    File *f = find_file(currentDir, nome_arquivo);
    if (!f) {
        if (currentDir->file_count >= MAX_CHILDREN) {
            printf("Erro: limite de arquivos atingido.\n");
            return;
        }
        f = create_file(nome_arquivo, currentDir, FILE_TEXT, 644);
        currentDir->files[currentDir->file_count++] = f;
    }

    if (!has_permission(f->perms, currentUser, 'w')) {
        printf("Permissao negada para escrita.\n");
        return;
    }

    // Substitui conteúdo em memória
    free(f->content);
    f->content = strdup(texto);
    f->size = strlen(texto);
    time(&f->modified_at);

    // Grava conteúdo também nos blocos do "disco" simulado
    store_file_in_blocks(f, texto);
}

// chmod NUM arquivo
void cmd_chmod(char *mode_str, char *name) {
    File *f = find_file(currentDir, name);
    if (!f) {
        printf("Erro: arquivo nao encontrado.\n");
        return;
    }
    int mode = atoi(mode_str);
    if (mode < 0 || mode > 777) {
        printf("Modo invalido.\n");
        return;
    }
    f->perms.mode = mode;
}

// rm <arquivo>
void cmd_rm(char *name) {
    File *f = find_file(currentDir, name);
    if (!f) {
        printf("Erro: arquivo nao encontrado.\n");
        return;
    }
    if (!has_permission(f->perms, currentUser, 'w')) {
        printf("Permissao negada para remover.\n");
        return;
    }

    // liberação de blocos (3.4)
    free_file_blocks(f);

    free(f->content);

    // remove da lista do diretório
    int idx = -1;
    for (int i = 0; i < currentDir->file_count; i++) {
        if (currentDir->files[i] == f) {
            idx = i;
            break;
        }
    }
    if (idx >= 0) {
        for (int i = idx; i < currentDir->file_count - 1; i++) {
            currentDir->files[i] = currentDir->files[i+1];
        }
        currentDir->file_count--;
    }

    free(f);
}

// mv fonte destino (pode ser rename no mesmo diretório)
void cmd_mv(char *src, char *dst) {
    File *f = find_file(currentDir, src);
    if (!f) {
        printf("Erro: arquivo nao encontrado.\n");
        return;
    }
    // Versão simples: só renomeia
    strncpy(f->name, dst, MAX_NAME);
    f->name[MAX_NAME - 1] = '\0';
}

// cp fonte destino (mesmo diretorio)
void cmd_cp(char *src, char *dst) {
    File *f = find_file(currentDir, src);
    if (!f) {
        printf("Erro: arquivo nao encontrado.\n");
        return;
    }
    if (currentDir->file_count >= MAX_CHILDREN) {
        printf("Erro: limite de arquivos atingido.\n");
        return;
    }

    File *copy = create_file(dst, currentDir, f->type, f->perms.mode);

    // reconstruir conteúdo do arquivo fonte
    char *src_data = NULL;
    if (f->block_count > 0) {
        src_data = read_file_from_blocks(f);
    } else if (f->content) {
        src_data = strdup(f->content);
    } else {
        src_data = strdup("");
    }

    copy->size = strlen(src_data);
    copy->content = strdup(src_data);
    store_file_in_blocks(copy, src_data);
    free(src_data);

    currentDir->files[currentDir->file_count++] = copy;
}

// ====================== PARSER ======================

void handle_command(char *line) {
    // tira \n
    line[strcspn(line, "\n")] = '\0';

    if (strncmp(line, "echo ", 5) == 0 && strstr(line, " > ") != NULL) {
        cmd_echo_redirect(line);
        return;
    }

    char *cmd = strtok(line, " ");
    if (!cmd) return;

    if (strcmp(cmd, "ls") == 0) {
        cmd_ls();
    } else if (strcmp(cmd, "mkdir") == 0) {
        char *name = strtok(NULL, " ");
        if (!name) {
            printf("Uso: mkdir <nome>\n");
        } else {
            cmd_mkdir(name);
        }
    } else if (strcmp(cmd, "cd") == 0) {
        char *arg = strtok(NULL, " ");
        if (!arg) {
            printf("Uso: cd <dir>|..|/\n");
        } else {
            cmd_cd(arg);
        }
    } else if (strcmp(cmd, "touch") == 0) {
        char *name = strtok(NULL, " ");
        if (!name) {
            printf("Uso: touch <arquivo>\n");
        } else {
            cmd_touch(name);
        }
    } else if (strcmp(cmd, "cat") == 0) {
        char *name = strtok(NULL, " ");
        if (!name) {
            printf("Uso: cat <arquivo>\n");
        } else {
            cmd_cat(name);
        }
    } else if (strcmp(cmd, "chmod") == 0) {
        char *mode = strtok(NULL, " ");
        char *name = strtok(NULL, " ");
        if (!mode || !name) {
            printf("Uso: chmod <modo> <arquivo>\n");
        } else {
            cmd_chmod(mode, name);
        }
    } else if (strcmp(cmd, "rm") == 0) {
        char *name = strtok(NULL, " ");
        if (!name) {
            printf("Uso: rm <arquivo>\n");
        } else {
            cmd_rm(name);
        }
    } else if (strcmp(cmd, "mv") == 0) {
        char *src = strtok(NULL, " ");
        char *dst = strtok(NULL, " ");
        if (!src || !dst) {
            printf("Uso: mv <src> <dst>\n");
        } else {
            cmd_mv(src, dst);
        }
    } else if (strcmp(cmd, "cp") == 0) {
        char *src = strtok(NULL, " ");
        char *dst = strtok(NULL, " ");
        if (!src || !dst) {
            printf("Uso: cp <src> <dst>\n");
        } else {
            cmd_cp(src, dst);
        }
    } else if (strcmp(cmd, "help") == 0) {
        printf("Comandos disponiveis:\n");
        printf("  ls\n");
        printf("  mkdir <nome>\n");
        printf("  cd <dir>|..|/\n");
        printf("  touch <arquivo>\n");
        printf("  echo TEXTO > arquivo\n");
        printf("  cat <arquivo>\n");
        printf("  chmod <modo> <arquivo>\n");
        printf("  rm <arquivo>\n");
        printf("  mv <src> <dst>\n");
        printf("  cp <src> <dst>\n");
        printf("  exit\n");
    } else if (strcmp(cmd, "") == 0) {
        // nada
    } else {
        printf("Comando desconhecido. Use 'help'.\n");
    }
}

// ====================== MAIN ======================

int main() {
    char line[MAX_INPUT];

    init_fs();

    printf("Mini Sistema de Arquivos em Memoria (M3 - SO)\n");
    printf("Digite 'help' para ajuda, 'exit' para sair.\n");

    while (1) {
        print_prompt();
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        if (strncmp(line, "exit", 4) == 0) {
            break;
        }
        handle_command(line);
    }

    return 0;
}
