_Este projeto foi criado como parte do currículo da 42 por nmatondo, ajacinto, emalungo._

# Webserver

## Descrição

O **Webserver** é um servidor HTTP implementado em C++98 conforme o padrão HTTP/1.1. O projeto tem como objetivo desenvolver um servidor web funcional capaz de processar múltiplas requisições simultâneas, servir arquivos estáticos, executar scripts CGI e gerenciar recursos de rede de forma eficiente.

Este projeto faz parte do currículo da 42 e visa aprofundar o conhecimento em programação de sistemas, redes de computadores, sockets, I/O multiplexing e manipulação de protocolos de rede. O servidor implementa funcionalidades essenciais encontradas em servidores web modernos como nginx e Apache, incluindo suporte a múltiplos hosts virtuais, redirecionamentos, tratamento de erros personalizados e execução de CGI.

### Funcionalidades Principais

- Tratamento de múltiplas conexões simultâneas usando `poll()` para multiplexing de I/O
- Suporte aos métodos HTTP: GET, POST e DELETE
- Leitura e interpretação de arquivos de configuração customizados
- Servir arquivos estáticos (HTML, CSS, JavaScript, imagens)
- Execução de scripts CGI (Common Gateway Interface)
- Suporte a múltiplos hosts virtuais
- Redirecionamentos HTTP
- Páginas de erro personalizadas
- Upload de arquivos
- Gerenciamento automático de tipos MIME

### Tecnologias e Conceitos

- **Linguagem**: C++98
- **Funções do sistema**: socket, bind, listen, accept, recv, send, fork, execve, dup2, pipe, close, chdir, poll
- **Protocolo**: HTTP/1.1 (RFC 2616 e RFC 7230-7235)
- **Arquitetura**: Event-driven com I/O não-bloqueante
- **Padrões**: Programação orientada a objetos para modularidade

## Instruções

### Compilação

Para compilar o projeto, execute:

```bash
make
```

O executável `webserv` será gerado no diretório raiz do projeto.

Para limpar os arquivos objeto:

```bash
make clean
```

Para limpar completamente (incluindo o executável):

```bash
make fclean
```

Para recompilar tudo:

```bash
make re
```

### Execução

Para iniciar o servidor, forneça um arquivo de configuração como argumento:

```bash
./webserv config/default.conf
```

O servidor lerá as configurações do arquivo especificado e iniciará os servidores virtuais definidos. Por padrão, o servidor escuta na porta 8080 e serve arquivos da pasta `www/`.

### Testando o Servidor

Você pode testar o servidor de várias formas:

**Usando curl:**
```bash
curl -v http://localhost:8080/
```

**Usando um navegador:**
```
http://localhost:8080
```

**Testando upload de arquivos:**
```bash
curl -X POST -F "file=@arquivo.txt" http://localhost:8080/upload
```

**Testando CGI:**
```bash
curl http://localhost:8080/cgi-bin/script.py
```

### Estrutura do Projeto

```
webserv/
├── Makefile
├── README.md
├── config/
│   └── default.conf
├── src/
│   ├── main.cpp
│   ├── server/
│   │   ├── Server.cpp
│   │   └── Client.cpp
│   ├── http/
│   │   ├── HttpRequest.cpp
│   │   ├── Response.cpp
│   │   ├── StatusCodes.cpp
│   │   └── HttpMethods.cpp
│   ├── config/
│   │   ├── ConfigParser.cpp
│   │   ├── ConfigValidator.cpp
│   │   └── ConfigHelper.cpp
│   ├── cgi/
│   │   └── EnvBuilder.cpp
│   └── utils/
│       ├── FileUtils.cpp
│       └── StringUtils.cpp
├── include/
│   └── [arquivos .hpp]
└── www/
    ├── index.html
    ├── errors/
    ├── uploads/
    └── cgi-bin/
```

## Recursos

### Referências Técnicas

- **RFC 2616** - Hypertext Transfer Protocol -- HTTP/1.1
- **RFC 7230-7235** - HTTP/1.1 Message Syntax and Routing
- **Beej's Guide to Network Programming** - Guia fundamental sobre sockets e programação de rede
- **NGINX Documentation** - Documentação do nginx para entender arquitetura de servidores modernos
- **Apache HTTP Server Documentation** - Referência sobre configuração e funcionamento de servidores HTTP
- **Man pages**: socket(2), bind(2), listen(2), accept(2), poll(2), send(2), recv(2), fork(2), execve(2)
- **CGI 1.1 Specification** - Especificação do Common Gateway Interface

### Tutoriais e Artigos

- "How to build a simple HTTP server" - Tutoriais sobre implementação de servidores HTTP
- "Understanding HTTP/1.1 Protocol" - Artigos sobre o protocolo HTTP
- "Multiplexing I/O with poll()" - Documentação sobre multiplexing de I/O
- Documentação do C++ Reference (cppreference.com) para funções padrão C++98

### Uso de IA

Durante o desenvolvimento deste projeto, ferramentas de IA foram utilizadas nas seguintes tarefas:

- **Esclarecimento de conceitos**: Consultas sobre detalhes técnicos do protocolo HTTP/1.1, interpretação de RFCs e funcionamento de system calls UNIX
- **Debugging**: Identificação de problemas em código específico, análise de comportamentos inesperados e sugestões de correção
- **Documentação**: Auxiliar na elaboração de comentários e documentação do código
- **Refatoração**: Sugestões de melhorias na estrutura do código e padrões de design

**Partes do projeto desenvolvidas com auxílio de IA:**
- Estruturação inicial da arquitetura do servidor
- Implementação do parser de configuração
- Tratamento de casos especiais do protocolo HTTP
- Otimização do gerenciamento de recursos e conexões

Todo código foi revisado, compreendido e adaptado manualmente para garantir conformidade com os requisitos do projeto e padrão C++98.
