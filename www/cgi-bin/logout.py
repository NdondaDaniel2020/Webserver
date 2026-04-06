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
        return {"ok": False, "error": "Nenhuma sessão ativa"}, "400 Bad Request", None
    
    try:
        conn = sqlite3.connect(SESSIONS_PATH)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM sessions WHERE session_token = ?", (session_token,))
        conn.commit()
        conn.close()
        
        return {"ok": True, "message": "Logout realizado com sucesso"}, "200 OK", session_token
    
    except Exception as e:
        return {"ok": False, "error": str(e)}, "500 Internal Server Error", None

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    if method != "POST":
        result = {"ok": False, "error": "Use POST para fazer logout"}
        status = "405 Method Not Allowed"
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
    main()
