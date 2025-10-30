# 🌐 Webserver – Servidor HTTP em C++

## 📖 Descrição

O **Webserver** é um projeto desenvolvido em **C++98** que tem como objetivo implementar um **servidor HTTP** leve e funcional, capaz de processar múltiplas conexões simultâneas e responder requisições de clientes (como navegadores) de acordo com o protocolo **HTTP/1.1**.  

Este projeto faz parte de um exercício de aprofundamento em **programação de sistemas** e **redes de computadores**, com foco na compreensão dos mecanismos internos de um servidor web moderno.

---

## ⚙️ Funcionalidades

- 🔁 Tratamento de múltiplas conexões usando **poll()**, **select()** ou **kqueue()**  
- 📡 Suporte aos métodos HTTP: `GET`, `POST`, e `DELETE`  
- ⚙️ Leitura e interpretação de arquivos de configuração (`.conf`)  
- 📂 Servir arquivos estáticos (HTML, CSS, imagens, etc.)  
- 🔀 Redirecionamentos e erros personalizados  
- 🌍 Suporte a múltiplos hosts virtuais  
- 🧾 Logs básicos de requisições e respostas  

---

## 🧠 Tecnologias e Conceitos Utilizados

- Linguagem: **C++98**
- Funções do sistema UNIX:  
  `socket`, `bind`, `listen`, `accept`, `recv`, `send`, `fork`, `execve`, `dup2`, `pipe`, `close`, `chdir`, `getaddrinfo`, etc.
- Manipulação de erros e códigos de status HTTP  
- Gestão eficiente de **recursos de rede e processos**  
- Programação orientada a objetos para modularidade e reusabilidade  

---

## 📁 Estrutura do Projeto

```bash
webserv/
│── Makefile                # Automação de compilação
│── README.md               # Documentação do projeto
│── config/                 # Arquivo(s) de configuração do servidor
│   ├── default.conf        # Arquivo de configuração padrão
│   └── examples/           # Exemplos de configs adicionais
│
│── src/                    # Código fonte (C++)
│   ├── main.cpp            # Ponto de entrada
│   ├── server/             # Lógica do servidor
│   │   ├── Server.cpp
│   │   ├── Client.cpp
│   │   └── PollManager.cpp
│   │
│   ├── http/               # Camada HTTP (parser e respostas)
│   │   ├── Request.cpp
│   │   ├── Response.cpp
│   │   ├── StatusCodes.cpp
│   │   └── Headers.cpp
│   │
│   ├── config/             # Parser do arquivo de configuração
│   │   ├── ConfigParser.cpp
│   │   └── ConfigData.cpp
│   │
│   ├── cgi/                # Módulo CGI
│   │   ├── CGIHandler.cpp
│   │   └── EnvBuilder.cpp
│   │
│   └── utils/              # Funções auxiliares
│       ├── Logger.cpp
│       └── FileUtils.cpp
│
│── include/                # Headers (.hpp ou .h)
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── PollManager.hpp
│   ├── Request.hpp
│   ├── Response.hpp
│   ├── StatusCodes.hpp
│   ├── Headers.hpp
│   ├── ConfigParser.hpp
│   ├── ConfigData.hpp
│   ├── CGIHandler.hpp
│   ├── EnvBuilder.hpp
│   ├── Logger.hpp
│   └── FileUtils.hpp
│
│── www/                    # Conteúdo servido pelo servidor
│   ├── index.html
│   ├── errors/
│   │   ├── 404.html
│   │   └── 500.html
│   ├── uploads/            # Pasta de uploads
│   └── cgi-bin/            # Scripts CGI (ex.: .php, .py)
│
│── tests/                  # Testes manuais e automáticos
│   ├── test_requests.http  # Arquivo com requests para curl/httpie
│   ├── test_upload.py      # Script para testar upload
│   ├── stress_test.py      # Stress test (múltiplas conexões)
│   └── unit/               # Se quiser unit tests em C++
│
└── docs/                   # Documentação
    ├── RFC_notes.md        # Notas de estudo HTTP
    └── design.md           # Arquitetura e decisões do projeto

```

---

## 🧩 Como Compilar

```bash
make
```

O executável principal será gerado no diretório raiz com o nome webserv.

---

## 🚀 Como Executar

```bash
./webserv config/default.conf
```

- O servidor usará as configurações definidas no arquivo .conf.
- Por padrão, servirá os arquivos contidos na pasta www/.

---

## 🧪 Testes

Para validar o comportamento do servidor:

```bash
curl -v http://localhost:8080/
```

Ou abra o navegador e acesse:
http://localhost:8080

---

## 🎯 Objetivo Final

O projeto busca replicar, em pequena escala, o comportamento de servidores reais como nginx ou Apache, fortalecendo o domínio sobre sockets, I/O não bloqueante, e programação orientada a objetos em baixo nível.

---

# 🪪 Licença

Este projeto é de uso educacional e segue a licença MIT.
