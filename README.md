# 🗂️ Sistema de Indexação de Documentos

Este projeto foi desenvolvido no âmbito da unidade curricular de **Sistemas Operativos**.  
Consiste num sistema cliente-servidor, implementado em **C**, que permite a **indexação, consulta, remoção e pesquisa de documentos** através de comunicação por **FIFOs com nome (named pipes)**. Inclui ainda **gestão de cache com política LRU** e exportação de estatísticas.

## 📌 Funcionalidades Implementadas

### 📥 Adição de Documentos (`-a`)
- Permite adicionar um documento com os seguintes campos:
  - Título
  - Autores
  - Ano de publicação
  - Caminho para o ficheiro do documento
- Os metadados são extraídos automaticamente do conteúdo, guardados e associados a um identificador único.

### 🔎 Consulta de Documentos (`-c`)
- Consulta os metadados de um documento a partir do seu identificador (`id`).
- Os dados apresentados incluem título, autores, ano e caminho do ficheiro.
- Integra gestão de **cache LRU**, distinguindo entre `HIT` e `MISS`.

### 📊 Contagem de Linhas com Palavra-chave (`-l`)
- Conta o número de linhas num documento que contêm uma palavra-chave.
- Implementado através de `fork` e `exec` com o comando `grep -c`.

### 🧠 Pesquisa Concorrente (`-s`)
- Pesquisa a palavra-chave em todos os documentos indexados usando múltiplos processos.
- Mostra o número de ocorrências por documento.
- Mede e apresenta o tempo de execução total da pesquisa.

### 🗑️ Remoção de Documento (`-d`)
- Permite remover um documento do índice, atualizando os dados persistentes.

### 🧼 Encerramento do Servidor (`-f`)
- Encerra de forma segura o servidor, garantindo a escrita dos dados persistentes.
- Exporta estatísticas da cache e o estado atual da cache para ficheiro.

---

## 🛠️ Estrutura do Projeto

O projeto está organizado de forma modular e cumpre todos os requisitos do enunciado:

📁 `src/` — Código-fonte:
- `dserver.c` — Implementação do servidor.
- `dclient.c` — Implementação do cliente.
- `index.c` — Gestão do índice de documentos e cache.
- `common.h` — Definições comuns (estruturas, constantes, enums).
- `server.h` / `client.h` / `index.h` — Headers específicos por módulo.

📁 `include/` — Headers para modularização.

📁 `bin/` — Executáveis compilados:
- `dserver`
- `dclient`

📁 `docs/` — Documentos a indexar (ficheiros `.txt`).

📁 `tmp/` — Diretório auxiliar para uso interno.

📄 `data/index.txt` — Metadados persistentes dos documentos.
📄 `data/cache_snapshot.txt` — Exportação dos IDs em cache (ordem LRU).
📄 `Makefile` — Compilação automática (`make` e `make debug`).

---

## 🚀 Como Executar

### 📦 Compilar o Projeto
```bash
make         # modo normal (sem debug)
make debug   # modo com logs da cache
```

### ▶️ Executar o Servidor
```bash
./bin/dserver docs 10
```
- O `10` representa o número máximo de documentos a manter em cache.

### 🧑‍💻 Executar o Cliente

#### Adicionar documento:
```bash
./bin/dclient -a "Romeo and Juliet" "William Shakespeare" "1997" "docs/1112.txt"
```

#### Consultar documento:
```bash
./bin/dclient -c 1
```

#### Contar linhas com palavra-chave:
```bash
./bin/dclient -l 1 "Romeo"
```

#### Pesquisar palavra-chave em todos:
```bash
./bin/dclient -s "Romeo" 4
```

#### Remover documento:
```bash
./bin/dclient -d 1
```

#### Encerrar servidor:
```bash
./bin/dclient -f
```

---

## 📈 Estado Atual do Projeto

| Comando | Estado | Observações |
|---------|--------|-------------|
| `-a`    | ✅     | Adição de documentos funcional |
| `-c`    | ✅     | Consulta de metadados com cache LRU |
| `-l`    | ✅     | Contagem com `grep` funcional |
| `-d`    | ✅     | Remoção funcional |
| `-s`    | ✅     | Pesquisa concorrente com tempo total |
| `-f`    | ✅     | Encerra servidor, guarda índice e cache |
| `Cache` | ✅     | LRU com exportação e estatísticas |

---

## 📜 Autores

Este projeto foi desenvolvido por:

- Hélder Tiago Peixoto da Cruz - A104174  
- André Miguel Rego Trindade Pinto - A104267  
- Rafael Airosa Pereira - A...
