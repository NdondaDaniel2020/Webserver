# Implementacao de CGI e Integracao com Response

## Objetivo

Este documento explica como o projeto executa scripts CGI, qual e o papel do namespace `CGIHandler`, como a classe `Response` decide quando usar CGI, e como a resposta final HTTP e montada.

Arquivos centrais:

- `include/CGIHandler.hpp`
- `src/cgi/CGIHandler.cpp`
- `include/EnvBuilder.hpp`
- `src/cgi/EnvBuilder.cpp`
- `include/Response.hpp`
- `src/http/Response.cpp`

---

## Visao Geral

O suporte a CGI foi implementado como um fluxo separado da entrega normal de arquivos estaticos.

O caminho geral e este:

```text
HttpRequest
  -> Response::buildHttpResponse()
  -> Response::methodGet() ou Response::methodPost()
  -> Response::isCgiRequest()
  -> CGIHandler::executeCgi()
  -> EnvBuilder::build()
  -> fork() + execve()
  -> leitura da saida do script
  -> montagem da resposta HTTP final
```

Quando o recurso pedido corresponde a uma extensao configurada em `location.cgi_handlers`, o servidor deixa de tratar esse recurso como arquivo estatico e passa a executa-lo como processo CGI.

---

## Porque foi usado um namespace

O modulo de CGI nao foi modelado como classe. Em vez disso, ele foi colocado no namespace `CGIHandler`.

Essa decisao faz sentido porque:

- o modulo nao guarda estado interno entre requisicoes;
- a API publica e pequena, com uma funcao principal: `executeCgi()`;
- as funcoes auxiliares (`getExtension`, `stripQuery`, `closePipes`) sao detalhes internos do arquivo `.cpp`;
- o namespace agrupa responsabilidade sem obrigar construcao de objeto.

Na pratica, `CGIHandler` funciona como um modulo utilitario especializado em executar scripts CGI e devolver uma resposta HTTP pronta.

---

## Interface publica do namespace CGIHandler

Em `include/CGIHandler.hpp`, a interface publica e:

```cpp
namespace CGIHandler {
    bool executeCgi(const HttpRequest &request,
                    const std::string &scriptPath,
                    const LocationConfig &location,
                    std::string &outResponse);
}
```

### Parametros

- `request`: contem metodo, URI, headers, body e demais dados da requisicao.
- `scriptPath`: caminho do script no filesystem.
- `location`: configuracao da location atual, incluindo o mapa `cgi_handlers`.
- `outResponse`: string de saida onde a resposta HTTP completa sera escrita.

### Retorno

- `true`: o CGI foi executado e `outResponse` contem uma resposta HTTP pronta.
- `false`: houve falha de configuracao ou de execucao.

Isso permite que `Response` apenas delegue a execucao e reaproveite o resultado diretamente em `response_str`.

---

## Como Response detecta uma requisicao CGI

O ponto de decisao fica em `Response::isCgiRequest()`.

Regras usadas hoje:

1. precisa existir uma `location` correspondente;
2. essa `location` precisa ter `cgi_handlers` configurado;
3. a URI precisa terminar com uma extensao;
4. a extensao precisa existir no mapa `location->cgi_handlers`.

Exemplo conceitual:

```text
location /cgi-bin {
    root /var/www/cgi-bin;
    cgi .py /usr/bin/python3;
    cgi .php /usr/bin/php-cgi;
}
```

Se a URI terminar com `.py`, `Response` entende que deve usar o interpretador associado a `.py` em vez de tentar servir o arquivo diretamente.

---

## Integracao com GET

No metodo `Response::methodGet()`, o teste de CGI acontece logo no inicio.

Fluxo atual:

1. encontra a `location` pelo URI;
2. chama `isCgiRequest(request.getUri(), location)`;
3. se for CGI, chama `CGIHandler::executeCgi()`;
4. se der certo, copia a resposta montada para `response_str` e encerra o metodo;
5. se falhar, devolve `500 Internal Server Error`.

Isso e importante porque o CGI tem prioridade sobre outras regras de GET, como:

- redirect da location;
- busca de index file;
- autoindex;
- leitura direta de arquivo estatico.

Em outras palavras: se o recurso e reconhecido como CGI, ele nao entra no fluxo normal de arquivo estatico.

---

## Integracao com POST

Em `Response::methodPost()`, o CGI tambem e tratado antes do restante do processamento do body.

Fluxo atual:

1. valida `client_max_body_size` global;
2. encontra a `location` correspondente;
3. se a `location` aceitar CGI para a extensao da URI, monta o `scriptPath`;
4. chama `CGIHandler::executeCgi()`;
5. se funcionar, usa a resposta devolvida pelo script;
6. se falhar, retorna `500 Internal Server Error`.

Se nao for CGI, o POST continua pelo fluxo normal de:

- `multipart/form-data`;
- `application/x-www-form-urlencoded`;
- `application/json`;
- `text/plain`.

Isso mostra que CGI e um caminho alternativo completo para POST, e nao apenas um detalhe de upload.

---

## Como o scriptPath e resolvido

Existem dois caminhos levemente diferentes no codigo:

### GET

No fluxo de GET, `buildHttpResponse()` calcula `file_path` com base em:

```text
root + removeLocationInUri(uri, location)
```

Esse valor e passado para `methodGet()`, que o reutiliza ao chamar o CGI.

### POST

No fluxo de POST, o caminho do script e reconstruido com:

```text
location->root + request.getUri().substr(location->path.size())
```

Os dois caminhos perseguem a mesma ideia: transformar a URI em um caminho real dentro da `root` da location.

Observacao importante: essa diferenca de montagem merece atencao em manutencao futura, porque duas estrategias diferentes para resolver o mesmo recurso podem gerar comportamento inconsistente em cenarios de alias ou normalizacao de path.

---

## Montagem do ambiente CGI com EnvBuilder

Antes de chamar `execve()`, o servidor monta um vetor de variaveis de ambiente com `EnvBuilder::build()`.

Variaveis definidas explicitamente hoje:

- `REDIRECT_STATUS=200`
- `REQUEST_METHOD`
- `QUERY_STRING`
- `SERVER_PROTOCOL`
- `GATEWAY_INTERFACE=CGI/1.1`
- `SERVER_SOFTWARE=webserv/1.0`
- `SCRIPT_FILENAME`
- `SCRIPT_NAME`
- `REQUEST_URI`
- `CONTENT_TYPE` quando existir
- `CONTENT_LENGTH` quando existir

Depois disso, todos os headers HTTP recebidos sao convertidos para o formato CGI:

```text
User-Agent: curl/8.5.0
-> HTTP_USER_AGENT=curl/8.5.0
```

Regras aplicadas:

- o nome do header e convertido para maiusculo;
- `-` vira `_`;
- `Content-Type` e `Content-Length` nao entram como `HTTP_*` porque ja sao adicionados separadamente.

Esse ambiente e entregue ao processo CGI como `envp` em `execve()`.

---

## Passo a passo dentro de CGIHandler::executeCgi()

### 1. Limpeza do caminho

O metodo remove a query string do caminho com `stripQuery()`.

Exemplo:

```text
/www/cgi-bin/hello.py?name=ana
-> /www/cgi-bin/hello.py
```

Isso garante que a extensao e o caminho do script sejam resolvidos corretamente.

### 2. Escolha do interpretador

`getExtension()` pega a extensao do arquivo, e o modulo procura essa extensao em `location.cgi_handlers`.

Se nao existir mapeamento para a extensao, a funcao retorna `false`.

Exemplo:

```text
.py  -> /usr/bin/python3
.php -> /usr/bin/php-cgi
```

### 3. Conversao do ambiente para `char **`

`EnvBuilder::build()` devolve `std::vector<std::string>`.

Em seguida, o codigo cria um vetor `std::vector<char *>` apontando para os buffers internos dessas strings, e adiciona `NULL` no final para cumprir a interface de `execve()`.

### 4. Montagem do argv

O processo filho recebe:

```text
argv[0] = interpretador
argv[1] = scriptPath
argv[2] = NULL
```

Ou seja, o servidor executa o interpretador e passa o script como primeiro argumento.

### 5. Criacao de pipes

Sao criados dois pipes:

- `inpipe`: servidor escreve o body da requisicao e o CGI le do `stdin`;
- `outpipe`: CGI escreve em `stdout` e `stderr`, e o servidor le esse conteudo.

### 6. fork()

Depois do `fork()`:

- o filho prepara `stdin`, `stdout` e `stderr` com `dup2()`;
- o pai escreve o body e le a saida do CGI.

### 7. Processo filho

No filho:

- `STDIN_FILENO` recebe `inpipe[0]`;
- `STDOUT_FILENO` recebe `outpipe[1]`;
- `STDERR_FILENO` tambem recebe `outpipe[1]`;
- todos os descritores redundantes sao fechados;
- `execve()` substitui o processo pelo interpretador.

Se `execve()` falhar, o filho termina com codigo `127`.

### 8. Processo pai

No pai:

- fecha a ponta de leitura de `inpipe`;
- fecha a ponta de escrita de `outpipe`;
- se o metodo for `POST`, escreve o body no `stdin` do CGI;
- fecha `inpipe[1]` para sinalizar fim de entrada;
- le tudo de `outpipe[0]` para `cgiOutput`;
- espera o filho com `waitpid()`.

Se o processo filho terminar com codigo `127`, o servidor considera a execucao um erro e retorna `false`.

---

## Como a saida do CGI vira resposta HTTP

Depois de capturar toda a saida do script, `CGIHandler::executeCgi()` tenta separar headers e body.

Separadores aceitos:

- `\r\n\r\n`
- `\n\n`

Se houver cabecalhos, o codigo procura uma linha especial:

```text
Status: 404 Not Found
```

Quando essa linha existe:

- o valor apos `Status:` vira a linha de status HTTP;
- os outros cabecalhos sao reaproveitados na resposta final.

Se nao houver `Status:`, o status padrao e:

```text
200 OK
```

Depois, o servidor monta a resposta final assim:

```text
HTTP/1.1 <status>
<headers vindos do CGI>
Content-Length: <tamanho do body>
Connection: close

<body>
```

Isso significa que o CGI nao entrega a resposta diretamente ao socket. Quem sempre envia a resposta final ao cliente continua sendo o servidor.

---

## Relacao entre Response e CGIHandler

As responsabilidades estao separadas desta forma:

### Response

- decide qual location corresponde a URI;
- valida metodo permitido;
- decide se a requisicao e CGI ou nao;
- escolhe entre fluxo de arquivo estatico, upload, delete, redirect ou CGI;
- armazena a resposta final em `response_str`.

### CGIHandler

- resolve extensao e interpretador;
- prepara ambiente e argumentos;
- cria pipes;
- faz `fork()` e `execve()`;
- injeta o body no `stdin` do script quando necessario;
- recolhe a saida do processo;
- transforma a saida do CGI em uma resposta HTTP pronta.

### EnvBuilder

- traduz `HttpRequest` para variaveis de ambiente no formato esperado por scripts CGI.

Essa divisao deixa a classe `Response` como orquestradora de alto nivel, e concentra a mecanica de processo no modulo CGI.

---

## Comportamento de erro atual

As principais falhas tratadas hoje sao:

- extensao sem interpretador configurado: `executeCgi()` retorna `false`;
- erro em `pipe()` ou `fork()`: `executeCgi()` retorna `false`;
- erro em `execve()`: o filho sai com `127`, e o pai converte isso em falha;
- falha de CGI no fluxo de GET: `Response` responde `500 Internal Server Error` com mensagem `CGI execution failed`;
- falha de CGI no fluxo de POST: `Response` responde `500 Internal Server Error` com mensagem `CGI Error`.

Ponto importante: `stderr` do CGI e redirecionado para o mesmo pipe de `stdout`. Isso ajuda no debug, mas tambem significa que mensagens de erro do script podem acabar misturadas ao conteudo de resposta se o script nao seguir o formato esperado.

---

## Limitacoes da implementacao atual

O modulo esta funcional, mas ainda tem limites claros:

1. o CGI e tratado de forma bloqueante;
2. nao existe timeout para script travado ou muito lento;
3. a resposta final sempre usa `Connection: close`;
4. GET e POST resolvem `scriptPath` por caminhos diferentes;
5. nao ha tratamento especifico para codigos de saida diferentes de `127`;
6. nao ha parsing avancado de headers CGI repetidos ou malformados;
7. `stderr` e `stdout` compartilham o mesmo fluxo.

Esses pontos nao invalidam a implementacao, mas devem ser conhecidos por quem for evoluir o modulo.

---

## Resumo tecnico

- `Response` decide se a requisicao entra no fluxo CGI.
- `CGIHandler` executa o script e monta a resposta HTTP.
- `EnvBuilder` converte a requisicao para variaveis de ambiente CGI.
- o namespace foi usado porque o modulo nao precisa de estado nem de instancias.
- a resposta devolvida pelo CGI e reaproveitada por `Response` como string HTTP completa.

Se o objetivo for evoluir esse modulo, o proximo passo natural e unificar a resolucao de `scriptPath`, adicionar timeout de execucao e separar melhor o tratamento de `stdout` e `stderr`.