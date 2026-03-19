#!/bin/bash

# ============================================================================
# TEST: Sistema de Sessões e Cookies
# Descrição: Valida criação, envio e manutenção de cookies de sessão
# ============================================================================

BASE_URL="http://127.0.0.1:8080"
COOKIE_FILE="/tmp/webserv_test_cookies.txt"
TEMP_OUTPUT="/tmp/webserv_test_output.txt"

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Contadores
TESTS_PASSED=0
TESTS_FAILED=0
TOTAL_TESTS=0

# ============================================================================
# Funções auxiliares
# ============================================================================

print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TEST $TOTAL_TESTS]${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓ PASS:${NC} $1"
    ((TESTS_PASSED++))
}

print_fail() {
    echo -e "${RED}✗ FAIL:${NC} $1"
    ((TESTS_FAILED++))
}

print_info() {
    echo -e "${BLUE}ℹ INFO:${NC} $1"
}

cleanup() {
    rm -f "$COOKIE_FILE" "$TEMP_OUTPUT"
}

# ============================================================================
# TESTES
# ============================================================================

print_header "TESTE DE COOKIES DE SESSÃO - WEBSERVER"

# Limpar cookies anteriores
cleanup

# ----------------------------------------------------------------------------
# TEST 1: Servidor está rodando
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Verificar se servidor está rodando"

if curl -s --connect-timeout 2 "$BASE_URL" > /dev/null 2>&1; then
    print_success "Servidor está acessível em $BASE_URL"
else
    print_fail "Servidor não está rodando em $BASE_URL"
    echo -e "\n${RED}Execute o servidor antes de rodar os testes:${NC}"
    echo -e "${YELLOW}./webserv config/default.conf${NC}\n"
    exit 1
fi

# ----------------------------------------------------------------------------
# TEST 2: Criação de cookie de sessão (primeira requisição sem cookie)
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Criar nova sessão (primeira requisição sem cookie)"

RESPONSE=$(curl -s -c "$COOKIE_FILE" -w "\n%{http_code}" \
    "$BASE_URL/cgi-bin/test_cookie.py")

HTTP_CODE=$(echo "$RESPONSE" | tail -n 1)
BODY=$(echo "$RESPONSE" | head -n -1)

if [ "$HTTP_CODE" = "200" ]; then
    print_success "Status HTTP 200 OK"
else
    print_fail "Status HTTP esperado 200, recebido $HTTP_CODE"
fi

# Verificar se cookie foi criado
if [ -f "$COOKIE_FILE" ] && grep -q "session_id" "$COOKIE_FILE"; then
    SESSION_ID=$(grep "session_id" "$COOKIE_FILE" | awk '{print $7}')
    print_success "Cookie 'session_id' criado: $SESSION_ID"
else
    print_fail "Cookie 'session_id' não foi criado"
fi

# Verificar resposta JSON
if echo "$BODY" | grep -q '"status": "success"'; then
    print_success "Resposta indica sucesso na criação"
else
    print_fail "Resposta não indica sucesso"
fi

if echo "$BODY" | grep -q '"message": "Novo cookie criado!"'; then
    print_success "Mensagem confirma novo cookie"
else
    print_fail "Mensagem incorreta na resposta"
fi

# ----------------------------------------------------------------------------
# TEST 3: Manutenção de sessão (segunda requisição com cookie)
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Manter sessão (segunda requisição com cookie)"

RESPONSE2=$(curl -s -b "$COOKIE_FILE" -w "\n%{http_code}" \
    "$BASE_URL/cgi-bin/test_cookie.py")

HTTP_CODE2=$(echo "$RESPONSE2" | tail -n 1)
BODY2=$(echo "$RESPONSE2" | head -n -1)

if [ "$HTTP_CODE2" = "200" ]; then
    print_success "Status HTTP 200 OK"
else
    print_fail "Status HTTP esperado 200, recebido $HTTP_CODE2"
fi

# Verificar se sessão foi reconhecida
if echo "$BODY2" | grep -q '"message": "Cookie recebido com sucesso!"'; then
    print_success "Sessão reconhecida pelo servidor"
else
    print_fail "Servidor não reconheceu a sessão"
fi

# Verificar se session_id é o mesmo
SESSION_ID_2=$(echo "$BODY2" | grep -o '"session_id": "[^"]*"' | cut -d'"' -f4)
if [ "$SESSION_ID" = "$SESSION_ID_2" ]; then
    print_success "Session ID mantido entre requisições: $SESSION_ID"
else
    print_fail "Session ID mudou (esperado: $SESSION_ID, recebido: $SESSION_ID_2)"
fi

# ----------------------------------------------------------------------------
# TEST 4: Ninja Arena - Criação de jogador com sessão
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Ninja Arena - Criar jogador e sessão"

ARENA_RESPONSE=$(curl -s -c "$COOKIE_FILE" -w "\n%{http_code}" \
    -X POST -d "action=new_game&name=TestWarrior&class=blade" \
    "$BASE_URL/cgi-bin/arena_action.py")

ARENA_HTTP=$(echo "$ARENA_RESPONSE" | tail -n 1)
ARENA_BODY=$(echo "$ARENA_RESPONSE" | head -n -1)

if [ "$ARENA_HTTP" = "200" ]; then
    print_success "Status HTTP 200 OK"
else
    print_fail "Status HTTP esperado 200, recebido $ARENA_HTTP"
fi

# Verificar se cookie NAGSID foi criado
if [ -f "$COOKIE_FILE" ] && grep -q "NAGSID" "$COOKIE_FILE"; then
    NAGSID=$(grep "NAGSID" "$COOKIE_FILE" | awk '{print $7}')
    print_success "Cookie 'NAGSID' criado: $NAGSID"
else
    print_fail "Cookie 'NAGSID' não foi criado"
fi

# Verificar criação do jogador
if echo "$ARENA_BODY" | grep -q '"name": "TestWarrior"'; then
    print_success "Jogador 'TestWarrior' criado"
else
    print_fail "Jogador não foi criado corretamente"
fi

# ----------------------------------------------------------------------------
# TEST 5: Ninja Arena - Recuperar estado com sessão
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Ninja Arena - Recuperar estado do jogador com cookie"

ARENA_STATE=$(curl -s -b "$COOKIE_FILE" \
    "$BASE_URL/cgi-bin/arena_action.py?action=state")

# Verificar se jogador foi recuperado
if echo "$ARENA_STATE" | grep -q '"name": "TestWarrior"'; then
    print_success "Estado do jogador recuperado: TestWarrior"
else
    print_fail "Não foi possível recuperar estado do jogador"
fi

# Verificar atributos do jogador
if echo "$ARENA_STATE" | grep -q '"class": "blade"'; then
    print_success "Classe do jogador mantida: blade"
else
    print_fail "Classe do jogador não foi mantida"
fi

if echo "$ARENA_STATE" | grep -q '"level": 1'; then
    print_success "Nível do jogador correto: 1"
else
    print_fail "Nível do jogador incorreto"
fi

# ----------------------------------------------------------------------------
# TEST 6: Validar atributos de segurança do cookie
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Validar atributos de segurança do cookie"

COOKIE_HEADERS=$(curl -s -v -X POST -d "action=new_game&name=SecurityTest&class=shadow" \
    "$BASE_URL/cgi-bin/arena_action.py" 2>&1 | grep -i "set-cookie")

if echo "$COOKIE_HEADERS" | grep -q "HttpOnly"; then
    print_success "Atributo HttpOnly presente"
else
    print_fail "Atributo HttpOnly ausente (vulnerabilidade XSS)"
fi

if echo "$COOKIE_HEADERS" | grep -q "SameSite"; then
    print_success "Atributo SameSite presente"
else
    print_fail "Atributo SameSite ausente (vulnerabilidade CSRF)"
fi

if echo "$COOKIE_HEADERS" | grep -q "Path=/"; then
    print_success "Atributo Path configurado corretamente"
else
    print_fail "Atributo Path não configurado"
fi

# ----------------------------------------------------------------------------
# TEST 7: Teste de múltiplas sessões simultâneas
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Múltiplas sessões simultâneas"

COOKIE_FILE_2="/tmp/webserv_test_cookies2.txt"

# Criar segunda sessão
SESSION_2=$(curl -s -c "$COOKIE_FILE_2" -X POST \
    -d "action=new_game&name=Player2&class=monk" \
    "$BASE_URL/cgi-bin/arena_action.py")

# Verificar se session IDs são diferentes
NAGSID_1=$(grep "NAGSID" "$COOKIE_FILE" | awk '{print $7}')
NAGSID_2=$(grep "NAGSID" "$COOKIE_FILE_2" | awk '{print $7}')

if [ "$NAGSID_1" != "$NAGSID_2" ]; then
    print_success "Sessões independentes criadas (IDs diferentes)"
else
    print_fail "Sessões compartilham o mesmo ID"
fi

# Verificar se cada sessão mantém seus próprios dados
STATE_1=$(curl -s -b "$COOKIE_FILE" "$BASE_URL/cgi-bin/arena_action.py?action=state")
STATE_2=$(curl -s -b "$COOKIE_FILE_2" "$BASE_URL/cgi-bin/arena_action.py?action=state")

if echo "$STATE_1" | grep -q '"name": "TestWarrior"' && \
   echo "$STATE_2" | grep -q '"name": "Player2"'; then
    print_success "Cada sessão mantém dados isolados"
else
    print_fail "Dados de sessão estão misturados"
fi

rm -f "$COOKIE_FILE_2"

# ----------------------------------------------------------------------------
# TEST 8: Requisição sem cookie retorna novo cookie
# ----------------------------------------------------------------------------
((TOTAL_TESTS++))
print_test "Requisição sem cookie gera novo cookie"

NO_COOKIE_RESPONSE=$(curl -s -v "$BASE_URL/cgi-bin/arena_action.py?action=state" 2>&1)

if echo "$NO_COOKIE_RESPONSE" | grep -q "Set-Cookie: NAGSID="; then
    print_success "Novo cookie gerado para requisição sem cookie"
else
    print_fail "Servidor não gerou cookie para requisição sem cookie"
fi

# ============================================================================
# RESUMO DOS TESTES
# ============================================================================

cleanup

print_header "RESUMO DOS TESTES"

echo -e "Total de testes: ${BLUE}$TOTAL_TESTS${NC}"
echo -e "Testes aprovados: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Testes falhados: ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}✓ TODOS OS TESTES PASSARAM!${NC}\n"
    exit 0
else
    echo -e "\n${RED}✗ ALGUNS TESTES FALHARAM${NC}\n"
    exit 1
fi
