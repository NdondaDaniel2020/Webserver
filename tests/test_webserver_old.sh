#!/bin/bash

################################################################################
# Script de Teste para Webserver
# Testa funcionalidades básicas do webserver
################################################################################

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
SERVER_HOST="127.0.0.1"
SERVER_PORT="8080"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"
CONFIG_FILE="config/default.conf"
SERVER_BIN="./webserv"
SERVER_PID=""
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

################################################################################
# Funções Utilitárias
################################################################################

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
    ((PASSED_TESTS++))
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
    ((FAILED_TESTS++))
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
    ((SKIPPED_TESTS++))
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

cleanup() {
    print_info "Finalizando testes..."
    if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        print_info "Encerrando servidor (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null
        sleep 1
        if kill -0 $SERVER_PID 2>/dev/null; then
            kill -9 $SERVER_PID 2>/dev/null
        fi
    fi
}

wait_for_server() {
    local attempts=0
    local max_attempts=30
    
    print_info "Aguardando servidor estar pronto..."
    while [ $attempts -lt $max_attempts ]; do
        if curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/" 2>/dev/null | grep -q "200\|301\|302\|404\|500"; then
            print_success "Servidor está pronto!"
            return 0
        fi
        sleep 0.5
        ((attempts++))
    done
    
    print_error "Servidor não respondeu após ${max_attempts}s"
    return 1
}

test_endpoint() {
    local method=$1
    local path=$2
    local expected_codes=$3
    local description=$4
    
    local response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$path" 2>/dev/null)
    local body=$(echo "$response" | head -n-1)
    local http_code=$(echo "$response" | tail -n1)
    
    # Convert expected codes to array and check if actual code is in the list
    local codes_array=($(echo "$expected_codes" | tr '|' ' '))
    local code_found=0
    
    for expected in "${codes_array[@]}"; do
        if [ "$http_code" = "$expected" ]; then
            code_found=1
            break
        fi
    done
    
    if [ $code_found -eq 1 ]; then
        print_success "$description (HTTP $http_code)"
        return 0
    else
        print_error "$description - Esperado um de: $expected_codes, Obtido: $http_code"
        return 1
    fi
}

test_endpoint_contains() {
    local method=$1
    local path=$2
    local expected_codes=$3
    local expected_content=$4
    local description=$5
    
    local response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$path" 2>/dev/null)
    local body=$(echo "$response" | head -n-1)
    local http_code=$(echo "$response" | tail -n1)
    
    # Convert expected codes to array and check if actual code is in the list
    local codes_array=($(echo "$expected_codes" | tr '|' ' '))
    local code_found=0
    
    for expected in "${codes_array[@]}"; do
        if [ "$http_code" = "$expected" ]; then
            code_found=1
            break
        fi
    done
    
    if [ $code_found -ne 1 ]; then
        print_error "$description - Esperado código um de: $expected_codes, Obtido: $http_code"
        return 1
    fi
    
    if echo "$body" | grep -q "$expected_content"; then
        print_success "$description (HTTP $http_code, contém '$expected_content')"
        return 0
    else
        print_error "$description - Conteúdo não contém: $expected_content"
        return 1
    fi
}

test_endpoint_headers() {
    local method=$1
    local path=$2
    local expected_code=$3
    local description=$4
    
    local response=$(curl -s -i -X "$method" "$BASE_URL$path" 2>/dev/null)
    local http_code=$(echo "$response" | head -n1 | grep -oP '\d{3}')
    
    if [ "$http_code" = "$expected_code" ]; then
        print_success "$description (HTTP $http_code)"
        echo "$response"
        return 0
    else
        print_error "$description - Esperado: $expected_code, Obtido: $http_code"
        return 1
    fi
}

################################################################################
# MAIN
################################################################################

trap cleanup EXIT

print_header "TESTE DO WEBSERVER"

# Verificação de Pré-requisitos
print_header "1. Verificação de Pré-requisitos"

if [ ! -f "$SERVER_BIN" ]; then
    print_info "Servidor não encontrado. Compilando..."
    if make clean && make; then
        print_success "Compilação concluída com sucesso"
    else
        print_error "Falha na compilação"
        exit 1
    fi
else
    print_success "Binário do servidor encontrado: $SERVER_BIN"
fi

if [ ! -f "$CONFIG_FILE" ]; then
    print_error "Arquivo de configuração não encontrado: $CONFIG_FILE"
    exit 1
fi
print_success "Arquivo de configuração encontrado: $CONFIG_FILE"

if ! command -v curl &> /dev/null; then
    print_error "curl não está instalado"
    exit 1
fi
print_success "curl está instalado"

# Iniciar Servidor
print_header "2. Iniciando o Servidor"

"$SERVER_BIN" "$CONFIG_FILE" > /tmp/webserver_test.log 2>&1 &
SERVER_PID=$!
print_info "Servidor iniciado com PID: $SERVER_PID"

# Aguardar servidor estar pronto
if ! wait_for_server; then
    print_error "Servidor não iniciou corretamente"
    cat /tmp/webserver_test.log
    exit 1
fi

sleep 1

# Testes Básicos de Conectividade
print_header "3. Testes de Conectividade"

test_endpoint GET "/" "200" "GET / - Página inicial"
test_endpoint GET "/index.html" "200" "GET /index.html"

# Testes de Erros
print_header "4. Testes de Páginas de Erro"

test_endpoint GET "/arquivo-nao-existe.html" "404" "GET /arquivo-inexistente (404)"
test_endpoint GET "/api/" "403|404" "GET /api/ (403 ou 404)"

# Testes de Conteúdo
print_header "5. Testes de Conteúdo"

if [ -f "www/index.html" ]; then
    test_endpoint_contains GET "/" "200" "<!DOCTYPE\|<html\|<head" "Verificar HTML válido na raiz"
else
    print_warning "Arquivo www/index.html não encontrado"
fi

# Testes de Métodos HTTP
print_header "6. Testes de Métodos HTTP"

# Test HEAD
response=$(curl -s -o /dev/null -w "%{http_code}" -X HEAD "$BASE_URL/" 2>/dev/null)
if [ "$response" = "405" ]; then
    print_warning "HEAD / - HTTP $response (método não suportado)"
elif [ "$response" = "200" ]; then
    print_success "HEAD / - HTTP $response"
else
    print_warning "HEAD / - HTTP $response"
fi

# Teste de POST (geralmente retorna 400 se Content-Type falta ou 405 se não configurado)
test_endpoint POST "/" "400|405|201|404" "POST / - Teste de método"

# Teste de DELETE (geralmente retorna 405 se não configurado ou 403 se proibido)
test_endpoint DELETE "/" "403|405|204|404" "DELETE / - Teste de método"

# Testes de Headers
print_header "7. Verificação de Headers"

print_info "Headers da resposta GET /:"
curl -s -i "$BASE_URL/" 2>/dev/null | head -n 10

# Teste de Performance Simples
print_header "8. Teste de Performance"

print_info "Fazendo 10 requisições sucessivas..."
success_count=0
for i in {1..10}; do
    response=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/" 2>/dev/null)
    if [ "$response" = "200" ]; then
        ((success_count++))
    fi
done
print_info "Sucesso: $success_count/10 requisições"
if [ $success_count -eq 10 ]; then
    print_success "Teste de performance OK"
else
    print_warning "Algumas requisições falharam"
fi

# Teste de Timeout
print_header "9. Teste de Conexão com Timeout"

timeout 5 curl -s "$BASE_URL/" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    print_success "Requisição completou dentro do timeout"
else
    print_error "Requisição expirou ou falhou"
fi

# Testes Adicionais
print_header "10. Testes de Directors e Estrutura"

# Teste de raiz (/)
test_endpoint GET "/" "200" "GET / - Página raiz"

# Teste de subdiretórios
test_endpoint GET "/text/" "200|403|404" "GET /text/ - Subdiretório"

# Testes de tipos de arquivo
if [ -f "www/index.html" ]; then
    test_endpoint GET "/index.html" "200" "GET /index.html"
fi

# Teste de File Upload (se existir diretório)
print_header "11. Teste de Redirecionamentos (se configurados)"

response=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/old-page" 2>/dev/null)
if [ "$response" = "301" ] || [ "$response" = "302" ]; then
    print_success "GET /old-page - Redirecionamento HTTP $response"
else
    print_info "GET /old-page - HTTP $response (redirecionamento não configurado ou diferente)"
fi

# Teste de HEAD com 200 OK (se suportado)
print_header "12. Testes Adicionais de Métodos"

response=$(curl -s -o /dev/null -w "%{http_code}" -X HEAD "$BASE_URL/index.html" 2>/dev/null)
if [ "$response" = "405" ]; then
    print_warning "HEAD /index.html - HTTP 405 (não suportado)"
elif [ "$response" = "200" ]; then
    print_success "HEAD /index.html - HTTP $response"
else
    print_info "HEAD /index.html - HTTP $response"
fi

# Resumo Final
print_header "RESUMO DOS TESTES"

total_tests=$((PASSED_TESTS + FAILED_TESTS + SKIPPED_TESTS))
echo -e "${GREEN}✓ Passaram:${NC}  $PASSED_TESTS"
echo -e "${RED}✗ Falharam:${NC}  $FAILED_TESTS"
echo -e "${YELLOW}⚠ Pulados:${NC}   $SKIPPED_TESTS"
echo -e "${BLUE}━ Total:${NC}     $total_tests"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    print_success "Todos os testes passaram! O servidor está funcionando corretamente. ✓"
    exit 0
else
    print_error "Alguns testes falharam. Veja os detalhes acima."
    exit 1
fi
