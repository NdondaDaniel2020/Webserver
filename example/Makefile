# Makefile para executar testes individuais
# Use: make 1, make 2, make 3, etc.

CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++98 -g
SRCDIR = .
BINDIR = bin

# Criar diretório bin se não existir
$(shell mkdir -p $(BINDIR))

# Lista de arquivos de teste
TEST1 = comunicacao_entre_processos_1.cpp
TEST2 = comunicacao_entre_processos_2.cpp
TEST3 = conectar_a_servidores.cpp
TEST4 = duplicacao_de_descritores.cpp
TEST5 = erros_e_mensagens.cpp
TEST6 = informacoes_de_ficheiros.cpp
TEST7 = manipulacao_diretorio.cpp
TEST8 = multiplexaCAo_clAssica_1.cpp
TEST9 = multiplexaCAo_clAssica_2.cpp
TEST10 = multiplexaCAo_clAssica_3.cpp
TEST11 = multiplexacao_com_epoll.cpp
TEST12 = rede_sockets.cpp
TEST13 = rede_sockets_avanCado_1.cpp
TEST14 = rede_sockets_avanCado_2.cpp
TEST15 = rede_sockets_avanCado_3.cpp
TEST16 = rede_sockets_avanCado_4.cpp
TEST17 = sinais_e_processos.cpp
TEST18 = sistema_de_arquivos_e_acesso.cpp
TEST19 = zbase_do_servidor_1.cpp
TEST20 = zbase_do_servidor_2.cpp

# Targets para cada teste
.PHONY: all clean help 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20

# Target padrão
all: help

help:
	@echo "=== Makefile para Testes de Rede ==="
	@echo "Uso: make [número_do_teste]"
	@echo ""
	@echo "Testes disponíveis:"
	@echo "  1  - comunicacao_entre_processos_1"
	@echo "  2  - comunicacao_entre_processos_2"
	@echo "  3  - conectar_a_servidores"
	@echo "  4  - duplicacao_de_descritores"
	@echo "  5  - erros_e_mensagens"
	@echo "  6  - informacoes_de_ficheiros"
	@echo "  7  - manipulacao_diretorio"
	@echo "  8  - multiplexaCAo_clAssica_1"
	@echo "  9  - multiplexaCAo_clAssica_2"
	@echo "  10 - multiplexaCAo_clAssica_3"
	@echo "  11 - multiplexacao_com_epoll"
	@echo "  12 - rede_sockets"
	@echo "  13 - rede_sockets_avanCado_1"
	@echo "  14 - rede_sockets_avanCado_2"
	@echo "  15 - rede_sockets_avanCado_3"
	@echo "  16 - rede_sockets_avanCado_4"
	@echo "  17 - sinais_e_processos"
	@echo "  18 - sistema_de_arquivos_e_acesso"
	@echo "  19 - zbase_do_servidor_1"
	@echo "  20 - zbase_do_servidor_2"
	@echo "Exemplo: make 1"
	@echo "         make clean"

# Teste 1
1: $(BINDIR)/test1
	@echo "=== Executando Teste 1: comunicacao_entre_processos_1 ==="
	./$(BINDIR)/test1

$(BINDIR)/test1: $(SRCDIR)/$(TEST1)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 2
2: $(BINDIR)/test2
	@echo "=== Executando Teste 2: comunicacao_entre_processos_2 ==="
	./$(BINDIR)/test2

$(BINDIR)/test2: $(SRCDIR)/$(TEST2)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 3
3: $(BINDIR)/test3
	@echo "=== Executando Teste 3: conectar_a_servidores ==="
	./$(BINDIR)/test3

$(BINDIR)/test3: $(SRCDIR)/$(TEST3)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 4
4: $(BINDIR)/test4
	@echo "=== Executando Teste 4: duplicacao_de_descritores ==="
	./$(BINDIR)/test4

$(BINDIR)/test4: $(SRCDIR)/$(TEST4)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 5
5: $(BINDIR)/test5
	@echo "=== Executando Teste 5: erros_e_mensagens ==="
	./$(BINDIR)/test5

$(BINDIR)/test5: $(SRCDIR)/$(TEST5)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 6
6: $(BINDIR)/test6
	@echo "=== Executando Teste 6: informacoes_de_ficheiros ==="
	./$(BINDIR)/test6

$(BINDIR)/test6: $(SRCDIR)/$(TEST6)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 7
7: $(BINDIR)/test7
	@echo "=== Executando Teste 7: manipulacao_diretorio ==="
	./$(BINDIR)/test7

$(BINDIR)/test7: $(SRCDIR)/$(TEST7)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 8
8: $(BINDIR)/test8
	@echo "=== Executando Teste 8: multiplexaCAo_clAssica_1 ==="
	./$(BINDIR)/test8

$(BINDIR)/test8: $(SRCDIR)/$(TEST8)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 9
9: $(BINDIR)/test9
	@echo "=== Executando Teste 9: multiplexaCAo_clAssica_2 ==="
	./$(BINDIR)/test9

$(BINDIR)/test9: $(SRCDIR)/$(TEST9)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 10
10: $(BINDIR)/test10
	@echo "=== Executando Teste 10: multiplexaCAo_clAssica_3 ==="
	./$(BINDIR)/test10

$(BINDIR)/test10: $(SRCDIR)/$(TEST10)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 11
11: $(BINDIR)/test11
	@echo "=== Executando Teste 11: multiplexacao_com_epoll ==="
	./$(BINDIR)/test11

$(BINDIR)/test11: $(SRCDIR)/$(TEST11)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 12
12: $(BINDIR)/test12
	@echo "=== Executando Teste 12: rede_sockets ==="
	./$(BINDIR)/test12

$(BINDIR)/test12: $(SRCDIR)/$(TEST12)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 13
13: $(BINDIR)/test13
	@echo "=== Executando Teste 13: rede_sockets_avanCado_1 ==="
	./$(BINDIR)/test13

$(BINDIR)/test13: $(SRCDIR)/$(TEST13)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 14
14: $(BINDIR)/test14
	@echo "=== Executando Teste 14: rede_sockets_avanCado_2 ==="
	./$(BINDIR)/test14

$(BINDIR)/test14: $(SRCDIR)/$(TEST14)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 15
15: $(BINDIR)/test15
	@echo "=== Executando Teste 15: rede_sockets_avanCado_3 ==="
	./$(BINDIR)/test15

$(BINDIR)/test15: $(SRCDIR)/$(TEST15)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 16
16: $(BINDIR)/test16
	@echo "=== Executando Teste 16: rede_sockets_avanCado_4 ==="
	./$(BINDIR)/test16

$(BINDIR)/test16: $(SRCDIR)/$(TEST16)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 17
17: $(BINDIR)/test17
	@echo "=== Executando Teste 17: sinais_e_processos ==="
	./$(BINDIR)/test17

$(BINDIR)/test17: $(SRCDIR)/$(TEST17)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Teste 18
18: $(BINDIR)/test18
	@echo "=== Executando Teste 18: sistema_de_arquivos_e_acesso ==="
	./$(BINDIR)/test18

19: $(BINDIR)/test19
	@echo "=== Executando Teste 19: zbase_do_servidor_1 ==="
	./$(BINDIR)/test19

20: $(BINDIR)/test20
	@echo "=== Executando Teste 20: zbase_do_servidor_2 ==="
	./$(BINDIR)/test20

$(BINDIR)/test18: $(SRCDIR)/$(TEST18)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Limpeza
clean:
	@echo "Limpando arquivos compilados..."
	rm -rf $(BINDIR)
	@echo "Limpeza concluída."

# Target para compilar todos os testes sem executar
compile-all: $(BINDIR)/test1 $(BINDIR)/test2 $(BINDIR)/test3 $(BINDIR)/test4 $(BINDIR)/test5 $(BINDIR)/test6 $(BINDIR)/test7 $(BINDIR)/test8 $(BINDIR)/test9 $(BINDIR)/test10 $(BINDIR)/test11 $(BINDIR)/test12 $(BINDIR)/test13 $(BINDIR)/test14 $(BINDIR)/test15 $(BINDIR)/test16 $(BINDIR)/test17 $(BINDIR)/test18)
	@echo "Todos os testes foram compilados."
