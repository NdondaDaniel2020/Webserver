#!/usr/bin/env python3
"""
verify_session.py — Verifica se o utilizador tem sessão válida
GET /cgi-bin/verify_session.py — pode ser usado para validar autenticação
"""

import os
import sqlite3
import sys
import json
import http.cookies
from datetime import datetime

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

def verify_session(session_token):
    """Verifica se sessão é válida e não expirou"""
    if not session_token:
        return None, False
    
    try:
        conn = sqlite3.connect(SESSIONS_PATH)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        cursor.execute(
            "SELECT * FROM sessions WHERE session_token = ? AND expires_at > datetime('now')",
            (session_token,)
        )
        session = cursor.fetchone()
        conn.close()
        
        if session:
            return dict(session), True
        return None, False
    
    except:
        return None, False

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    if method != "GET":
        result = {"ok": False, "error": "Use GET para verificar sessão", "authenticated": False}
        status = "405 Method Not Allowed"
    else:
        session_token = get_session_token_from_cookie()
        session_data, is_valid = verify_session(session_token)
        
        if is_valid and session_data:
            result = {
                "ok": True,
                "authenticated": True,
                "username": session_data['username'],
                "user_id": session_data['user_id'],
                "expires_at": session_data['expires_at']
            }
            status = "200 OK"
        else:
            result = {
                "ok": False,
                "authenticated": False,
                "error": "Sessão inválida ou expirada"
            }
            status = "401 Unauthorized"
    
    # Output HTTP headers and JSON
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n")
    sys.stdout.write("Access-Control-Allow-Origin: *\r\n")
    sys.stdout.write("Cache-Control: no-cache, no-store, must-revalidate\r\n")
    sys.stdout.write("Pragma: no-cache\r\n")
    sys.stdout.write("Expires: 0\r\n\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()
