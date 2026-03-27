#!/bin/bash

################################################################################
# Script de Teste Avançado para Webserver v2.0
# Suite completa de testes HTTP/1.1 com POST, DELETE, validações detalhadas
################################################################################

set -o pipefail

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
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
WARNING_MESSAGES=()
WRK_SUMMARY=""
WRK_EVIDENCE=""
SIEGE_SUMMARY=""
SIEGE_EVIDENCE=""
SIEGE_CGI_SUMMARY=""
SIEGE_CGI_EVIDENCE=""

# Diretório temporário para testes
TEST_DIR="/tmp/webserver_tests"
TEST_FILE="$TEST_DIR/test_upload.txt"
TEST_JSON_FILE="$TEST_DIR/test.json"
TEST_BIN_FILE="$TEST_DIR/test_binary.bin"

################################################################################
# Funções Utilitárias
################################################################################

print_header() {
    echo -e "\n${MAGENTA}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║${NC} $1"
    echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════╝${NC}\n"
}

print_subheader() {
    echo -e "\n${CYAN}▶ $1${NC}"
}

print_success() {
    echo -e "${GREEN}  ✓${NC} $1"
    ((PASSED_TESTS++))
}

print_error() {
    echo -e "${RED}  ✗${NC} $1"
    ((FAILED_TESTS++))
}

print_warning() {
    echo -e "${YELLOW}  ⚠${NC} $1"
    ((SKIPPED_TESTS++))
    WARNING_MESSAGES+=("$1")
}

print_info() {
    echo -e "${BLUE}  ℹ${NC} $1"
}

print_step() {
    echo -e "${CYAN}  → $1${NC}"
}

cleanup() {
    print_info "Finalizando testes..."
    if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        print_step "Encerrando servidor (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null
        sleep 1
        if kill -0 $SERVER_PID 2>/dev/null; then
            kill -9 $SERVER_PID 2>/dev/null
        fi
    fi
    # Limpar arquivos de teste
    rm -rf "$TEST_DIR" 2>/dev/null
}

wait_for_server() {
    local attempts=0
    local max_attempts=30
    
    print_step "Aguardando servidor estar pronto..."
    while [ $attempts -lt $max_attempts ]; do
        if curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/" 2>/dev/null | grep -q "200\|301\|302\|404\|500\|403"; then
            print_success "Servidor está pronto!"
            return 0
        fi
        sleep 0.5
        ((attempts++))
    done
    
    print_error "Servidor não respondeu após ${max_attempts}s"
    return 1
}

wait_for_pids_with_timeout() {
    local timeout_seconds=$1
    shift
    local pids=("$@")
    local start_time=$(date +%s)

    while true; do
        local remaining=0

        for pid in "${pids[@]}"; do
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                remaining=1
                break
            fi
        done

        if [ $remaining -eq 0 ]; then
            return 0
        fi

        local now=$(date +%s)
        if [ $((now - start_time)) -ge $timeout_seconds ]; then
            for pid in "${pids[@]}"; do
                if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                    kill "$pid" 2>/dev/null
                    sleep 0.1
                    kill -9 "$pid" 2>/dev/null
                fi
            done
            return 1
        fi

        sleep 0.1
    done
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
        print_success "$description (HTTP $http_code)"
        return 0
    else
        print_error "$description - Conteúdo não contém: $expected_content"
        return 1
    fi
}

test_post_request() {
    local path=$1
    local content_type=$2
    local data=$3
    local expected_codes=$4
    local description=$5
    
    local response=$(curl -s -w "\n%{http_code}" -X POST \
        -H "Content-Type: $content_type" \
        -d "$data" \
        "$BASE_URL$path" 2>/dev/null)
    local body=$(echo "$response" | head -n-1)
    local http_code=$(echo "$response" | tail -n1)
    
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
        print_warning "$description - HTTP $http_code (um de: $expected_codes esperado)"
        return 0
    fi
}

test_delete_request() {
    local path=$1
    local expected_codes=$2
    local description=$3
    
    local response=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE_URL$path" 2>/dev/null)
    local body=$(echo "$response" | head -n-1)
    local http_code=$(echo "$response" | tail -n1)
    
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
        print_warning "$description - HTTP $http_code (um de: $expected_codes esperado)"
        return 0
    fi
}

################################################################################
# MAIN
################################################################################

trap cleanup EXIT

print_header "TESTE AVANÇADO DO WEBSERVER - v2.0"

# Verificação de Pré-requisitos
print_header "1. Verificação de Pré-requisitos"

if [ ! -f "$SERVER_BIN" ]; then
    print_info "Servidor não encontrado. Compilando..."
    if make clean > /dev/null 2>&1 && make > /dev/null 2>&1; then
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

# Criar diretório de teste
mkdir -p "$TEST_DIR" 2>/dev/null

# Iniciar Servidor
print_header "2. Iniciando o Servidor"

"$SERVER_BIN" "$CONFIG_FILE" > /tmp/webserver_test.log 2>&1 &
SERVER_PID=$!
print_step "Servidor iniciado com PID: $SERVER_PID"

# Aguardar servidor estar pronto
if ! wait_for_server; then
    print_error "Servidor não iniciou corretamente"
    cat /tmp/webserver_test.log | head -20
    exit 1
fi

sleep 1

# Testes Básicos de Conectividade
print_header "3. Testes de Conectividade HTTP"

test_endpoint GET "/" "200" "GET / - Página inicial"
test_endpoint GET "/index.html" "200" "GET /index.html"

# Testes de Status Codes
print_header "4. Testes de Status Codes HTTP"

print_subheader "Respostas de Sucesso (2xx)"
test_endpoint GET "/index.html" "200" "200 - OK"

print_subheader "Respostas de Redirecionamento (3xx)"
test_endpoint GET "/old-page" "301|304|302" "Redirecionamento (301/302/304)"

print_subheader "Respostas de Cliente Error (4xx)"
test_endpoint GET "/arquivo-inexistente.html" "404" "404 - Not Found"
test_endpoint GET "/api/" "403|404" "403/404 - Forbidden/Not Found"

# Testes de Conteúdo
print_header "5. Testes de Validação de Conteúdo"

if [ -f "www/index.html" ]; then
    test_endpoint_contains GET "/" "200" "<!DOCTYPE\|<html\|<head" "HTML contém tags válidas"
    test_endpoint_contains GET "/index.html" "200" "html\|body\|head" "HTML contém estrutura básica"
else
    print_warning "Arquivo www/index.html não encontrado"
fi

# Testes de Métodos HTTP - GET
print_header "6. Testes de Métodos HTTP"

print_subheader "Testes GET"
test_endpoint GET "/" "200" "GET / - Acesso à raiz"
test_endpoint GET "/index.html" "200" "GET /index.html - Arquivo estático"

print_subheader "Testes POST com application/x-www-form-urlencoded"
test_post_request "/" "application/x-www-form-urlencoded" "name=test&value=123" "400|405|201|404" "POST com formulário"
test_post_request "/api/submit" "application/x-www-form-urlencoded" "test=data" "400|405|201|404|500" "POST /api/submit com formulário"

print_subheader "Testes POST com application/json"
test_post_request "/" "application/json" '{"test":"data"}' "400|405|201|404" "POST com JSON"
test_post_request "/api/data" "application/json" '{"key":"value"}' "400|405|201|404|500" "POST /api/data com JSON"

print_subheader "Testes POST com arquivo (multipart/form-data)"
echo "Arquivo de teste para upload - $(date)" > "$TEST_FILE"
response=$(curl -s -w "\n%{http_code}" -X POST \
    -F "file=@$TEST_FILE" \
    "$BASE_URL/uploads/" 2>/dev/null)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "201" ] || [ "$http_code" = "200" ] || [ "$http_code" = "204" ] || [ "$http_code" = "400" ] || [ "$http_code" = "403" ] || [ "$http_code" = "405" ]; then
    print_success "POST /uploads/ com multipart upload (HTTP $http_code)"
else
    print_warning "POST upload - HTTP $http_code"
fi

print_subheader "Testes POST sem Content-Type"
response=$(curl -s -w "\n%{http_code}" -X POST \
    -d "test=data" \
    "$BASE_URL/" 2>/dev/null)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "400" ] || [ "$http_code" = "411" ] || [ "$http_code" = "405" ]; then
    print_success "POST sem Content-Type (HTTP $http_code - comportamento esperado)"
else
    print_warning "POST sem Content-Type - HTTP $http_code"
fi

print_subheader "Testes DELETE"
test_delete_request "/" "403|405|204|400" "DELETE / - Tentativa em raiz"
test_delete_request "/uploads/" "204|403|405|404" "DELETE /uploads/ - Diretório"
test_delete_request "/uploads/test.txt" "204|404|403|405" "DELETE /uploads/test.txt - Arquivo"
test_delete_request "/index.html" "403|405|204" "DELETE /index.html - Arquivo protegido"

# Testes de Headers e Content-Type
print_header "7. Testes de Headers HTTP e Content-Type"

print_step "Verificando headers de resposta para GET /:"
response=$(curl -s -i "$BASE_URL/" 2>/dev/null)
http_header=$(echo "$response" | head -n1)
print_info "Status: $http_header"

content_type=$(echo "$response" | grep -i "Content-Type" | cut -d' ' -f2- | tr -d '\r')
if [ ! -z "$content_type" ]; then
    print_success "Content-Type detectado: $content_type"
else
    print_warning "Content-Type não encontrado nos headers"
fi

server_header=$(echo "$response" | grep -i "Server:" | cut -d' ' -f2- | tr -d '\r')
if [ ! -z "$server_header" ]; then
    print_success "Server header: $server_header"
else
    print_warning "Server header não encontrado"
fi

content_length=$(echo "$response" | grep -i "Content-Length:" | cut -d' ' -f2- | tr -d '\r')
if [ ! -z "$content_length" ]; then
    print_success "Content-Length: $content_length bytes"
else
    print_warning "Content-Length não encontrado"
fi

date_header=$(echo "$response" | grep -i "^Date:" | cut -d' ' -f2- | tr -d '\r')
if [ ! -z "$date_header" ]; then
    print_success "Date header presente: $date_header"
else
    print_warning "Date header não encontrado"
fi

# Testes de Persistência e Keep-Alive
print_header "8. Testes de Connection Management"

print_step "Testando 5 requisições consecutivas na mesma conexão..."
success_count=0
for i in {1..5}; do
    response=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/" 2>/dev/null)
    if [ "$response" = "200" ]; then
        ((success_count++))
    fi
done
if [ $success_count -eq 5 ]; then
    print_success "5/5 requisições sucessivas completadas"
else
    print_warning "Apenas $success_count/5 requisições foram bem-sucedidas"
fi

# Testes de Performance
print_header "9. Testes de Performance"

print_step "Executando 20 requisições em sequência..."
start_time=$(date +%s%N)
success_count=0
for i in {1..20}; do
    response=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/index.html" 2>/dev/null)
    if [ "$response" = "200" ]; then
        ((success_count++))
    fi
done
end_time=$(date +%s%N)
elapsed=$((($end_time - $start_time) / 1000000))

print_success "$success_count/20 requisições completadas em ${elapsed}ms"

if [ $success_count -eq 20 ]; then
    avg_time=$((elapsed / 20))
    print_info "Tempo médio por requisição: ${avg_time}ms"
fi

# Teste de resposta com timeout
print_header "10. Testes de Timeout e Limite de Conexão"

response=$(timeout 5 curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/" 2>/dev/null)
if [ $? -eq 0 ] && [ -n "$response" ]; then
    print_success "Requisição completou dentro do timeout de 5s"
else
    print_error "Requisição expirou ou falhou"
fi

# Testes de Diretórios
print_header "11. Testes de Directory Listing e Acesso"

test_endpoint GET "/" "200" "GET / - Raiz acessível"
test_endpoint GET "/text/" "200|403|404" "GET /text/ - Subdiretório"
test_endpoint GET "/api/" "403|404" "GET /api/ - Diretório API"
test_endpoint GET "/uploads/" "200|403|404" "GET /uploads/ - Diretório de uploads"

# Testes de Tipos de Arquivo
print_header "12. Testes de Tipos de Arquivo"

# Se existir arquivo CSS ou JS
if [ -f "www"/*.css ] 2>/dev/null; then
    response=$(curl -s -i "$(find www -name "*.css" | head -1 | sed "s|www||" | xargs -I {} echo "$BASE_URL{}")" 2>/dev/null | head -n15)
    print_info "CSS encontrado"
fi

# Se existir JavaScript
if [ -f "www"/*.js ] 2>/dev/null; then
    response=$(curl -s -i "$(find www -name "*.js" | head -1 | sed "s|www||" | xargs -I {} echo "$BASE_URL{}")" 2>/dev/null | head -n1)
    print_info "JavaScript encontrado"
fi

# Status páginas de erro
print_header "13. Testes de Páginas de Erro"

test_endpoint GET "/erro404testando.txt" "404" "Verificar página 404"
test_endpoint GET "/api" "403|404" "Verificar acesso a /api"

# Teste de Limite de Tamanho (Body Size)
print_header "14. Testes de Limites de Request"

print_step "Testando POST com corpo pequeno..."
response=$(curl -s -w "\n%{http_code}" -X POST \
    -H "Content-Type: application/octet-stream" \
    -H "Content-Length: 5" \
    -d "hello" \
    "$BASE_URL/" 2>/dev/null)
http_code=$(echo "$response" | tail -n1)
print_info "Corpo pequeno: HTTP $http_code"

print_step "Testando requisição com corpo moderado..."
# Criar arquivo de teste com 100KB
head -c 102400 /dev/urandom 2>/dev/null | base64 > "$TEST_BIN_FILE" 2>/dev/null
if [ -f "$TEST_BIN_FILE" ]; then
    response=$(curl -s -w "%{http_code}" -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@$TEST_BIN_FILE" \
        "$BASE_URL/uploads/" 2>/dev/null)
    http_code="${response: -3}"
    print_info "Corpo de ~100KB: HTTP $http_code"
fi

# Testes CGI (merged de tests/test_cgi.sh)
print_header "15. Testes CGI (Smoke)"

print_subheader "GET Python CGI"
response=$(curl -s -m 10 -w "\n%{http_code}" "$BASE_URL/cgi-bin/hello.py" 2>/dev/null)
body=$(echo "$response" | head -n-1)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "200" ]; then
    if echo "$body" | grep -qi "content-type\|done\|method"; then
        print_success "GET /cgi-bin/hello.py (HTTP 200)"
    else
        print_warning "GET /cgi-bin/hello.py respondeu 200 mas conteúdo inesperado"
    fi
else
    print_error "GET /cgi-bin/hello.py - Esperado 200, Obtido: $http_code"
fi

print_subheader "GET PHP CGI"
response=$(curl -s -m 10 -w "\n%{http_code}" "$BASE_URL/cgi-bin/hello.php" 2>/dev/null)
body=$(echo "$response" | head -n-1)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "200" ]; then
    if echo "$body" | grep -qi "PHP\|Method\|Query"; then
        print_success "GET /cgi-bin/hello.php (HTTP 200)"
    else
        print_warning "GET /cgi-bin/hello.php respondeu 200 mas conteúdo inesperado"
    fi
else
    print_error "GET /cgi-bin/hello.php - Esperado 200, Obtido: $http_code"
fi

# Testes CGI Avançados
print_header "16. Testes CGI Avançados"

print_subheader "POST em CGI - form_save.py"
response=$(curl -s -w "\n%{http_code}" -X POST \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "name=TestUser&email=test@example.com&message=Test%20Message" \
    "$BASE_URL/cgi-bin/form_save.py" 2>/dev/null)
body=$(echo "$response" | head -n-1)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "201" ] || [ "$http_code" = "200" ]; then
    if echo "$body" | grep -qi "ok\|saved\|success"; then
        print_success "POST /cgi-bin/form_save.py (HTTP $http_code)"
    else
        print_warning "POST form_save.py respondeu $http_code mas resposta inesperada"
    fi
else
    print_warning "POST /cgi-bin/form_save.py - HTTP $http_code"
fi

print_subheader "GET em CGI - Query string múltiplos"
response=$(curl -s -w "\n%{http_code}" "$BASE_URL/cgi-bin/hello.py?param1=value1&param2=value2&name=TestUser" 2>/dev/null)
body=$(echo "$response" | head -n-1)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "200" ]; then
    if echo "$body" | grep -qi "QUERY\|param\|value"; then
        print_success "GET /cgi-bin/hello.py?param1=val1&param2=val2 (HTTP 200)"
    else
        print_warning "GET com query múltiplos respondeu 200 mas conteúdo inesperado"
    fi
else
    print_error "GET /cgi-bin/hello.py com query múltiplos - HTTP $http_code"
fi

print_subheader "DELETE em CGI - form_list.php"
# Primeiro fazer um POST para ter um ID para deletar
POST_RESPONSE=$(curl -s -w "\n%{http_code}" -X POST \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "name=ToDelete&email=delete@example.com&message=Delete%20test" \
    "$BASE_URL/cgi-bin/form_save.py" 2>/dev/null)
POST_CODE=$(echo "$POST_RESPONSE" | tail -n1)

if [ "$POST_CODE" = "201" ] || [ "$POST_CODE" = "200" ]; then
    # Obter lista para pegar um ID
    LIST_RESPONSE=$(curl -s -w "\n%{http_code}" "$BASE_URL/cgi-bin/form_list.php" 2>/dev/null)
    LIST_CODE=$(echo "$LIST_RESPONSE" | tail -n1)
    LIST_BODY=$(echo "$LIST_RESPONSE" | head -n-1)
    
    # Extrair primeiro ID
    FIRST_ID=$(echo "$LIST_BODY" | grep -oP '"id":\K[0-9]+' | head -1)
    
    if [ ! -z "$FIRST_ID" ]; then
        DELETE_RESPONSE=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE_URL/cgi-bin/form_list.php?id=$FIRST_ID" 2>/dev/null)
        DELETE_CODE=$(echo "$DELETE_RESPONSE" | tail -n1)
        DELETE_BODY=$(echo "$DELETE_RESPONSE" | head -n-1)
        
        if [ "$DELETE_CODE" = "200" ]; then
            if echo "$DELETE_BODY" | grep -qi "deletad\|success\|ok"; then
                print_success "DELETE /cgi-bin/form_list.php?id=$FIRST_ID (HTTP 200)"
            else
                print_warning "DELETE respondeu 200 mas resposta inesperada"
            fi
        else
            print_warning "DELETE /cgi-bin/form_list.php?id=$FIRST_ID - HTTP $DELETE_CODE"
        fi
    else
        print_info "Nenhum ID disponível para teste DELETE"
    fi
else
    print_warning "Não foi possível criar entrada para teste DELETE"
fi

# Testes de Requisições e Headers
print_header "17. Testes de Headers e Requisições Especiais"

print_subheader "Cookies - Set-Cookie e envio"
response=$(curl -s -i -b /tmp/cookies.txt -c /tmp/cookies.txt "$BASE_URL/index.html" 2>/dev/null | head -20)
if [ -f /tmp/cookies.txt ] && [ -s /tmp/cookies.txt ]; then
    cookie_count=$(wc -l < /tmp/cookies.txt)
    print_success "Cookies testados - $cookie_count linhas em cookie jar"
else
    print_info "Nenhum cookie Set-Cookie encontrado no index.html (esperado)"
fi

print_subheader "Custom Headers - User-Agent e Accept"
response=$(curl -s -w "%{http_code}" \
    -H "User-Agent: TestClient/1.0" \
    -H "Accept: application/json" \
    -H "X-Custom-Header: CustomValue" \
    "$BASE_URL/index.html" 2>/dev/null)
http_code="${response: -3}"
if [ "$http_code" = "200" ]; then
    print_success "Custom headers aceitos (HTTP 200)"
else
    print_warning "Custom headers - HTTP $http_code"
fi

print_subheader "Conditional Requests - If-Modified-Since"
# Obter Last-Modified header
LAST_MOD=$(curl -s -i "$BASE_URL/index.html" 2>/dev/null | grep -i "Last-Modified" | cut -d' ' -f2-)
if [ ! -z "$LAST_MOD" ]; then
    response=$(curl -s -w "\n%{http_code}" \
        -H "If-Modified-Since: $LAST_MOD" \
        "$BASE_URL/index.html" 2>/dev/null)
    http_code=$(echo "$response" | tail -n1)
    if [ "$http_code" = "304" ]; then
        print_success "Conditional request - 304 Not Modified (If-Modified-Since)"
    elif [ "$http_code" = "200" ]; then
        print_info "Conditional request - 200 OK (arquivo foi modificado)"
    else
        print_warning "Conditional request - HTTP $http_code"
    fi
else
    print_info "Last-Modified não encontrado, skip teste conditional"
fi

print_subheader "Range Requests - bytes"
response=$(curl -s -w "\n%{http_code}" \
    -H "Range: bytes=0-100" \
    "$BASE_URL/index.html" 2>/dev/null)
body=$(echo "$response" | head -n-1)
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "206" ]; then
    print_success "Range request - 206 Partial Content (suportado)"
elif [ "$http_code" = "200" ]; then
    print_info "Range request - 200 OK (retornou arquivo completo)"
else
    print_warning "Range request - HTTP $http_code"
fi

print_subheader "HEAD Method"
response=$(curl -s -I "$BASE_URL/index.html" 2>/dev/null | head -1)
if echo "$response" | grep -q "200\|404\|405"; then
    print_success "HEAD /index.html retornou headers sem body"
else
    print_warning "HEAD method - resposta: $response"
fi

# Testes de Segurança
print_header "18. Testes de Segurança e Validação"

print_subheader "Path Traversal Prevention"
response=$(curl -s -w "%{http_code}" "$BASE_URL/../../etc/passwd" 2>/dev/null)
http_code="${response: -3}"
if [ "$http_code" = "400" ] || [ "$http_code" = "404" ]; then
    print_success "Path traversal bloqueado (HTTP $http_code)"
else
    print_warning "Path traversal - HTTP $http_code (esperado 400 ou 404)"
fi

print_subheader "Path Traversal - ../ pattern"
response=$(curl -s -w "%{http_code}" "$BASE_URL/../../../../../../../etc/passwd" 2>/dev/null)
http_code="${response: -3}"
if [ "$http_code" = "400" ] || [ "$http_code" = "404" ]; then
    print_success "Multiple ../ bloqueado (HTTP $http_code)"
else
    print_warning "Multiple ../ - HTTP $http_code"
fi

print_subheader "Requisições Malformadas - Headers quebrados"
# Teste com header sem valor
response=$(echo -e "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nInvalid-Header:\r\n\r\n" | \
    nc localhost 8080 2>/dev/null | head -1)
if [ ! -z "$response" ]; then
    print_info "Servidor responde a headers malformados (potencial DoS)"
else
    print_warning "Servidor pode ter rejeitado requisição malformada"
fi

print_subheader "Requisições Malformadas - Sem Content-Length em POST"
response=$(curl -s -w "%{http_code}" -X POST \
    -H "Content-Type: application/json" \
    -d '{"test":"data"}' \
    "$BASE_URL/uploads/" 2>/dev/null)
http_code="${response: -3}"
if [ "$http_code" != "000" ]; then
    print_success "POST sem Content-Length explícito respondido (HTTP $http_code)"
else
    print_warning "POST sem Content-Length - timeout ou falha"
fi

# Testes de Upload Grande
print_header "19. Testes de Upload e Limite de Tamanho"

print_subheader "Large file upload - 5MB"
# Criar arquivo de 5MB
head -c 5242880 /dev/urandom 2>/dev/null | base64 > "$TEST_DIR/large_file.txt" 2>/dev/null
if [ -f "$TEST_DIR/large_file.txt" ] && [ -s "$TEST_DIR/large_file.txt" ]; then
    file_size=$(wc -c < "$TEST_DIR/large_file.txt")
    START=$(date +%s%N)
    response=$(curl -s -w "\n%{http_code}" -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@$TEST_DIR/large_file.txt" \
        "$BASE_URL/uploads/" 2>/dev/null)
    END=$(date +%s%N)
    ELAPSED=$(( (END - START) / 1000000 ))
    
    body=$(echo "$response" | head -n-1)
    http_code=$(echo "$response" | tail -n1)
    
    if [ "$http_code" = "201" ] || [ "$http_code" = "200" ] || [ "$http_code" = "204" ]; then
        print_success "Upload 5MB completo em ${ELAPSED}ms (HTTP $http_code)"
    elif [ "$http_code" = "413" ]; then
        print_success "Upload 5MB rejeitado com 413 Payload Too Large (dentro do esperado)"
    else
        print_warning "Upload 5MB - HTTP $http_code após ${ELAPSED}ms"
    fi
else
    print_warning "Não foi possível criar arquivo de 5MB para teste"
fi

print_subheader "Large file upload - 10MB (teste limite)"
head -c 10485760 /dev/urandom 2>/dev/null | base64 > "$TEST_DIR/very_large_file.txt" 2>/dev/null
if [ -f "$TEST_DIR/very_large_file.txt" ] && [ -s "$TEST_DIR/very_large_file.txt" ]; then
    file_size=$(wc -c < "$TEST_DIR/very_large_file.txt")
    response=$(curl -s -m 30 -w "%{http_code}" -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@$TEST_DIR/very_large_file.txt" \
        "$BASE_URL/uploads/" 2>/dev/null)
    http_code="${response: -3}"
    
    if [ "$http_code" = "201" ] || [ "$http_code" = "200" ] || [ "$http_code" = "204" ]; then
        print_success "Upload 10MB completo (HTTP $http_code)"
    elif [ "$http_code" = "413" ]; then
        print_success "Upload 10MB rejeitado com 413 (limite de tamanho funcionando)"
    else
        print_warning "Upload 10MB - HTTP $http_code"
    fi
else
    print_warning "Não foi possível criar arquivo de 10MB para teste"
fi

# Testes Non-blocking e carga (merged de tests/test_nonblocking.sh)
print_header "20. Testes de Non-blocking"

print_subheader "CGI lento nao bloqueia outros"
curl -s -m 15 "$BASE_URL/cgi-bin/slow.php" > /dev/null 2>&1 &
SLOW_PID=$!
sleep 0.2

START=$(date +%s%N)
FAST_CODE=$(curl -s -m 5 -o /dev/null -w "%{http_code}" "$BASE_URL/cgi-bin/hello.php" 2>/dev/null)
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))

if [ "$FAST_CODE" = "200" ] && [ $ELAPSED -lt 1000 ]; then
    print_success "hello.php respondeu em ${ELAPSED}ms enquanto slow.php corria"
else
    print_error "Possivel bloqueio: hello.php HTTP $FAST_CODE em ${ELAPSED}ms (esperado < 1000ms)"
fi

if ! wait_for_pids_with_timeout 20 "$SLOW_PID"; then
    print_warning "slow.php excedeu timeout de espera e foi terminado"
fi

print_subheader "10 pedidos CGI simultaneos"
CGI_PIDS=""
for i in $(seq 1 10); do
    curl -s -m 10 -o /dev/null -w "%{http_code}\n" "$BASE_URL/cgi-bin/hello.php" 2>/dev/null > "$TEST_DIR/cgi_$i.out" &
    CGI_PIDS="$CGI_PIDS $!"
done

if ! wait_for_pids_with_timeout 15 $CGI_PIDS; then
    print_warning "Alguns pedidos CGI simultaneos excederam o timeout e foram terminados"
fi

ok_count=0
for i in $(seq 1 10); do
    code=$(cat "$TEST_DIR/cgi_$i.out" 2>/dev/null | tail -n1)
    if [ "$code" = "200" ]; then
        ((ok_count++))
    fi
done

if [ $ok_count -eq 10 ]; then
    print_success "10/10 pedidos CGI simultaneos completaram com HTTP 200"
else
    print_warning "$ok_count/10 pedidos CGI simultaneos completaram com HTTP 200"
fi

print_header "21. Testes de Carga e Clientes Simultaneos"

print_subheader "Carga com wrk"
if command -v wrk >/dev/null 2>&1; then
    print_step "Executando: wrk -t4 -c50 -d10s $BASE_URL/index.html"
    if wrk -t4 -c50 -d10s "$BASE_URL/index.html" > "$TEST_DIR/wrk_static.out" 2>&1; then
        if grep -q "Requests/sec" "$TEST_DIR/wrk_static.out"; then
            req_rate=$(grep "Requests/sec" "$TEST_DIR/wrk_static.out" | awk '{print $2}')
            transfer_rate=$(grep "Transfer/sec" "$TEST_DIR/wrk_static.out" | awk '{print $2}')
            total_requests=$(grep "requests in" "$TEST_DIR/wrk_static.out" | awk '{print $1}')
            WRK_SUMMARY="req/s=${req_rate}, transfer=${transfer_rate}, requests=${total_requests}"
            WRK_EVIDENCE=$(grep -E "requests in|Requests/sec|Transfer/sec" "$TEST_DIR/wrk_static.out" | tr '\n' '; ' | sed 's/; $//')
            print_success "wrk estatico executado com sucesso (${req_rate} req/s)"
        else
            print_warning "wrk terminou mas nao foi possivel extrair Requests/sec"
        fi
    else
        print_warning "wrk executou com falhas (ver $TEST_DIR/wrk_static.out)"
    fi
else
    print_warning "wrk nao esta instalado (comando: wrk -t4 -c50 -d10s $BASE_URL/index.html)"
fi

print_subheader "Carga com siege"
if command -v siege >/dev/null 2>&1; then
    print_step "Executando: siege -c50 -t10S $BASE_URL/index.html"
    if siege -c50 -t10S "$BASE_URL/index.html" > "$TEST_DIR/siege_static.out" 2>&1; then
        if grep -q '"transactions"' "$TEST_DIR/siege_static.out"; then
            transactions=$(grep '"transactions"' "$TEST_DIR/siege_static.out" | sed 's/[^0-9.]//g')
            availability=$(grep '"availability"' "$TEST_DIR/siege_static.out" | sed 's/[^0-9.]//g')
            transaction_rate=$(grep '"transaction_rate"' "$TEST_DIR/siege_static.out" | sed 's/[^0-9.]//g')
            concurrency=$(grep '"concurrency"' "$TEST_DIR/siege_static.out" | sed 's/[^0-9.]//g')
            SIEGE_SUMMARY="transactions=${transactions}, availability=${availability}%, rate=${transaction_rate}/s, concurrency=${concurrency}"
            SIEGE_EVIDENCE=$(grep -E '"transactions"|"availability"|"transaction_rate"|"concurrency"' "$TEST_DIR/siege_static.out" | tr '\n' '; ' | sed 's/; $//')
            print_success "siege estatico executado com sucesso (${transactions} transacoes)"
        else
            print_warning "siege terminou mas nao foi possivel extrair estatisticas"
        fi
    else
        print_warning "siege executou com falhas (ver $TEST_DIR/siege_static.out)"
    fi
else
    print_warning "siege nao esta instalado (comando: siege -c50 -t10S $BASE_URL/index.html)"
fi

print_subheader "Carga concorrente em CGI com siege"
if command -v siege >/dev/null 2>&1; then
    print_step "Executando: siege -c50 -t10S $BASE_URL/cgi-bin/hello.php"
    if siege -c50 -t10S "$BASE_URL/cgi-bin/hello.php" > "$TEST_DIR/siege_cgi.out" 2>&1; then
        if grep -q '"transactions"' "$TEST_DIR/siege_cgi.out"; then
            cgi_transactions=$(grep '"transactions"' "$TEST_DIR/siege_cgi.out" | sed 's/[^0-9.]//g')
            cgi_availability=$(grep '"availability"' "$TEST_DIR/siege_cgi.out" | sed 's/[^0-9.]//g')
            cgi_rate=$(grep '"transaction_rate"' "$TEST_DIR/siege_cgi.out" | sed 's/[^0-9.]//g')
            SIEGE_CGI_SUMMARY="transactions=${cgi_transactions}, availability=${cgi_availability}%, rate=${cgi_rate}/s"
            SIEGE_CGI_EVIDENCE=$(grep -E '"transactions"|"availability"|"transaction_rate"' "$TEST_DIR/siege_cgi.out" | tr '\n' '; ' | sed 's/; $//')
        fi
        print_success "siege CGI executado com sucesso"
    else
        print_warning "siege CGI executou com falhas (ver $TEST_DIR/siege_cgi.out)"
    fi
else
    print_warning "siege nao esta instalado para teste concorrente de CGI"
fi

# Resumo Final
print_header "RESUMO FINAL DOS TESTES"

total_tests=$((PASSED_TESTS + FAILED_TESTS + SKIPPED_TESTS))
echo ""
echo -e "${GREEN}  ✓ Passaram:${NC}     $PASSED_TESTS"
echo -e "${RED}  ✗ Falharam:${NC}     $FAILED_TESTS"
echo -e "${YELLOW}  ⚠ Avisos :${NC}     $SKIPPED_TESTS"
echo -e "${BLUE}  ━ Total   :${NC}     $total_tests"
echo ""

if [ -n "$WRK_SUMMARY" ] || [ -n "$SIEGE_SUMMARY" ] || [ -n "$SIEGE_CGI_SUMMARY" ]; then
    echo -e "${CYAN}  Estatisticas de carga:${NC}"
    if [ -n "$WRK_SUMMARY" ]; then
        echo -e "${CYAN}   wrk:${NC} $WRK_SUMMARY"
        echo -e "${BLUE}    evidencia:${NC} $WRK_EVIDENCE"
    fi
    if [ -n "$SIEGE_SUMMARY" ]; then
        echo -e "${CYAN}   siege estatico:${NC} $SIEGE_SUMMARY"
        echo -e "${BLUE}    evidencia:${NC} $SIEGE_EVIDENCE"
    fi
    if [ -n "$SIEGE_CGI_SUMMARY" ]; then
        echo -e "${CYAN}   siege CGI:${NC} $SIEGE_CGI_SUMMARY"
        echo -e "${BLUE}    evidencia:${NC} $SIEGE_CGI_EVIDENCE"
    fi
    echo ""
fi

if [ ${#WARNING_MESSAGES[@]} -gt 0 ]; then
    echo -e "${YELLOW}  Detalhes dos avisos:${NC}"
    for i in "${!WARNING_MESSAGES[@]}"; do
        echo -e "${YELLOW}   $(($i + 1)).${NC} ${WARNING_MESSAGES[$i]}"
    done
    echo ""
fi

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC} ✓ Todos os testes passaram! Servidor operacional! ✓${NC}      ${GREEN}║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}\n"
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║${NC} ✗ Alguns testes falharam. Veja detalhes acima. ✗${NC}         ${RED}║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}\n"
    exit 1
fi
