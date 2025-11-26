Mini Sistema de Arquivos em Memória
Trabalho M3 – Sistemas Operacionais
UNIVALI – Universidade do Vale do Itajaí
Professor: Michael D. C. Alves

Acadêmicos:
- Rhichyllie Baptista Stefen
- Leonardo Beduschi
- Luan de Melo

----------------------------------------------------

1. Introdução

Este projeto implementa um Mini Sistema de Arquivos inteiramente em memória,
com o objetivo de simular conceitos fundamentais de Sistemas Operacionais,
como diretórios estruturados em árvore, arquivos, permissões e alocação de blocos.

Nada é gravado no disco real; toda a simulação é feita em RAM.

----------------------------------------------------

2. Como compilar

Entre no diretório do projeto e use:

gcc mini_fs.c -o mini_fs

----------------------------------------------------

3. Como executar

./mini_fs

Ao iniciar, o programa exibe um prompt semelhante ao Linux:

Mini Sistema de Arquivos em Memoria (M3 - SO)
Digite 'help' para ajuda, 'exit' para sair.
/

----------------------------------------------------

4. Estrutura de Dados e Decisões de Design

O sistema de arquivos usa structs para representar diretórios e arquivos.
Cada diretório possui:
- nome
- ponteiro para o diretório pai
- lista de subdiretórios
- lista de arquivos

A escolha de árvore hierárquica reflete exatamente o modelo usado
por sistemas Unix-like.

Os arquivos usam uma estrutura equivalente a um FCB (File Control Block) 
ou inode simplificado, contendo:

- nome
- id único (inode)
- tipo
- tamanho
- permissões
- datas de criação, modificação e acesso
- conteúdo
- blocos alocados

Esta estrutura imita diretamente como sistemas reais controlam
metadados e localização dos dados.

----------------------------------------------------

5. Permissões RWX

O sistema usa o modelo chmod numérico:

Exemplo:
644  ->  rw- r-- r--

As permissões são verificadas antes de qualquer operação.
A função has_permission verifica se o usuário atual (OWNER)
possui direito de leitura, escrita ou execução.

----------------------------------------------------

6. Simulação de Disco e Alocação de Blocos

O disco simulado tem:

- 8192 bytes
- dividido em blocos de 64 bytes
- total de 128 blocos

Cada arquivo pode ocupar até 16 blocos.

A gravação funciona assim:

1. Divide o texto em pedaços de 64 bytes
2. Aloca blocos contínuos
3. Copia o conteúdo para o array DISK
4. Guarda os índices dos blocos dentro do arquivo

As funções principais são:

store_file_in_blocks
read_file_from_blocks
free_file_blocks

Isso demonstra de forma prática como um Sistema Operacional
converte dados lógicos (texto) em blocos físicos de armazenamento.

----------------------------------------------------

7. Comandos suportados

ls
mkdir nome
cd nome | .. | /
touch arquivo
echo TEXTO > arquivo
cat arquivo
chmod modo arquivo
rm arquivo
mv origem destino
cp origem destino
exit

Todos os comandos incluem tratamento de erros:
- permissões insuficientes
- arquivo não encontrado
- diretório inexistente
- limite de arquivos atingido
- uso incorreto do comando

----------------------------------------------------

8. Exemplos práticos

mkdir projetos
cd projetos
touch notas.txt
echo Ola Mundo > notas.txt
cat notas.txt
chmod 600 notas.txt
rm notas.txt

----------------------------------------------------

9. Conceitos demonstrados (conexão com a disciplina)

- Conceito de arquivo e atributos
- File Control Block (inode)
- Árvore de diretórios
- Permissões RWX com bitmask
- Operações fundamentais: criar, apagar, ler e escrever
- Alocação contínua de blocos de disco
- Tratamento de erros e mensagens claras
- Uso de ponteiros, structs e alocação dinâmica em C

----------------------------------------------------

10. Conclusão

O projeto demonstra todos os requisitos do Trabalho M3:

- Estrutura de diretórios em árvore
- Arquivos com FCB e inode
- Permissões RWX
- Alocação real de blocos
- Simulação fiel dos comandos do Linux
- Código limpo, comentado e modular

----------------------------------------------------


