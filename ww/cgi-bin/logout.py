#!/usr/bin/env python3
"""
logout.py — Termina sessão do utilizador
POST /cgi-bin/logout.py
"""

import os
import sqlite3
import sys
import json
import http.cookies

SESSIONS_PATH = "/tmp/sessions.db"

def get_session_token_from_cookie():
    """Extrai session token do header Cookie"""
    cookie_header = os.environ.get("HTTP_COOKIE", "")
    if not cookie_header:
        return None
    
    try:
        cookie = http.cookies.SimpleCookie(cookie_header)
        if 'session' in cookie:
            return cookie['session'].value
    except:
        pass
    
    return None

def logout_user(session_token):
    """Remove sessão da base de dados"""
    if not session_token:
        return {"ok": False, "error": "Nenhuma sessão ativa"}, "200 OK", None
    
    try:
        # Verificar se a base de dados existe
        if not os.path.exists(SESSIONS_PATH):
            return {"ok": False, "error": "Base de dados não encontrada"}, "200 OK", None
        
        conn = sqlite3.connect(SESSIONS_PATH)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM sessions WHERE session_token = ?", (session_token,))
        conn.commit()
        conn.close()
        
        return {"ok": True, "message": "Logout realizado com sucesso"}, "200 OK", session_token
    
    except Exception as e:
        # Retornar como sucesso mesmo com erro, para não bloquear logout
        # O cookie será removido pelo navegador
        sys.stderr.write(f"[logout.py] Erro na base de dados: {str(e)}\n")
        return {"ok": True, "message": "Logout processado"}, "200 OK", session_token

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    if method != "POST":
        result = {"ok": False, "error": "Use POST para fazer logout"}
        status = "200 OK"  # Retornar 200 OK para evitar que servidor retorne HTML de erro
        clear_cookie = False
    else:
        session_token = get_session_token_from_cookie()
        result, status, session = logout_user(session_token)
        clear_cookie = (session is not None)
    
    # Output HTTP headers and JSON
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n")
    sys.stdout.write("Access-Control-Allow-Origin: *\r\n")
    sys.stdout.write("Cache-Control: no-cache, no-store, must-revalidate\r\n")
    sys.stdout.write("Pragma: no-cache\r\n")
    sys.stdout.write("Expires: 0\r\n")
    
    # Limpa cookie de sessão
    if clear_cookie:
        cookie = http.cookies.SimpleCookie()
        cookie['session'] = ''
        cookie['session']['path'] = '/'
        cookie['session']['max-age'] = 0
        cookie['session']['expires'] = 'Thu, 01 Jan 1970 00:00:00 GMT'
        cookie['session']['samesite'] = 'Lax'
        sys.stdout.write(f"{cookie.output()}\r\n")
    
    sys.stdout.write("\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        # Garantir que sempre retornamos JSON, mesmo em caso de erro crítico
        sys.stdout.write("Status: 200 OK\r\n")
        sys.stdout.write("Content-Type: application/json\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.write(json.dumps({
            "ok": False,
            "error": f"Erro no servidor: {str(e)}"
        }, ensure_ascii=False))
        sys.stderr.write(f"[logout.py] Erro crítico: {str(e)}\n")
