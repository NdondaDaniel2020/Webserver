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
webserver/
├── src/              # Código-fonte principal (classes e lógica do servidor)
├── includes/         # Arquivos de cabeçalho (.hpp)
├── config/           # Arquivo(s) de configuração do servidor
├── www/              # Diretório raiz dos arquivos servidos
├── tests/            # Casos de teste e scripts de validação
├── Makefile          # Automação de compilação
└── README.md         # Documentação do projeto
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
