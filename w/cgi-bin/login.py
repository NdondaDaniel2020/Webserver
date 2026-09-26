#!/usr/bin/env python3
"""
login.py — Autentica utilizador e cria sessão com cookie
POST /cgi-bin/login.py (username, password obrigatórios)
"""

import os
import sqlite3
import sys
import json
import hashlib
import secrets
import http.cookies
import warnings
from urllib.parse import parse_qs
from datetime import datetime, timedelta

warnings.filterwarnings("ignore")

DB_PATH = "/tmp/users.db"
SESSIONS_PATH = "/tmp/sessions.db"

def init_db():
    """Cria tabelas se não existirem"""
    # Tabela de utilizadores
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()
    
    # Tabela de sessões
    conn = sqlite3.connect(SESSIONS_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_token TEXT UNIQUE NOT NULL,
            user_id INTEGER NOT NULL,
            username TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            expires_at DATETIME NOT NULL,
            FOREIGN KEY(user_id) REFERENCES users(id)
        )
    """)
    conn.commit()
    conn.close()

def verify_password(password, hash_stored):
    """Verifica password comparando com hash armazenado"""
    try:
        salt, hash_hex = hash_stored.split('$')
        hash_obj = hashlib.pbkdf2_hmac('sha256', password.encode(), salt.encode(), 100000)
        return hash_obj.hex() == hash_hex
    except:
        return False

def create_session(user_id, username):
    """Cria nova sessão e retorna token"""
    session_token = secrets.token_hex(32)
    expires_at = (datetime.now() + timedelta(days=7)).strftime("%Y-%m-%d %H:%M:%S")
    
    try:
        conn = sqlite3.connect(SESSIONS_PATH)
        conn.execute(
            "INSERT INTO sessions (session_token, user_id, username, expires_at) VALUES (?, ?, ?, ?)",
            (session_token, user_id, username, expires_at)
        )
        conn.commit()
        conn.close()
        return session_token
    except Exception as e:
        return None

def clean_expired_sessions():
    """Remove sessões expiradas"""
    try:
        conn = sqlite3.connect(SESSIONS_PATH)
        conn.execute("DELETE FROM sessions WHERE expires_at < datetime('now')")
        conn.commit()
        conn.close()
    except:
        pass

def login_user(username, password):
    """POST: autentica utilizador e cria sessão"""
    
    # Validações
    if not username or not password:
        return None, {"ok": False, "error": "username e password são obrigatórios"}, "400 Bad Request"
    
    username = username.strip()
    password = password.strip()
    
    try:
        conn = sqlite3.connect(DB_PATH)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        cursor.execute("SELECT id, username, password_hash FROM users WHERE username = ?", (username,))
        user = cursor.fetchone()
        conn.close()
        
        if not user:
            return None, {"ok": False, "error": "Utilizador ou password inválidos"}, "401 Unauthorized"
        
        if not verify_password(password, user['password_hash']):
            return None, {"ok": False, "error": "Utilizador ou password inválidos"}, "401 Unauthorized"
        
        # Criar sessão
        session_token = create_session(user['id'], user['username'])
        if not session_token:
            return None, {"ok": False, "error": "Erro ao criar sessão"}, "500 Internal Server Error"
        
        return session_token, {"ok": True, "message": "Login realizado com sucesso", "username": user['username']}, "200 OK"
    
    except Exception as e:
        return None, {"ok": False, "error": str(e)}, "500 Internal Server Error"

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    init_db()
    clean_expired_sessions()
    
    # Apenas POST é aceito
    if method != "POST":
        result = {"ok": False, "error": "Use POST para fazer login"}
        status = "405 Method Not Allowed"
        session_token = None
    else:
        # Parse POST body
        try:
            cl = int(os.environ.get("CONTENT_LENGTH", "0"))
        except ValueError:
            cl = 0
        
        params = {}
        if cl > 0:
            try:
                body_params = parse_qs(sys.stdin.read(cl), keep_blank_values=True)
                params.update(body_params)
            except:
                pass
        
        username = params.get("username", [""])[0].strip()
        password = params.get("password", [""])[0].strip()
        
        session_token, result, status = login_user(username, password)
    
    # Output HTTP headers and JSON
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n")
    sys.stdout.write("Access-Control-Allow-Origin: *\r\n")
    sys.stdout.write("Cache-Control: no-cache, no-store, must-revalidate\r\n")
    sys.stdout.write("Pragma: no-cache\r\n")
    sys.stdout.write("Expires: 0\r\n")
    
    # Adiciona cookie se login foi bem-sucedido
    if session_token:
        cookie = http.cookies.SimpleCookie()
        cookie['session'] = session_token
        cookie['session']['path'] = '/'
        cookie['session']['max-age'] = 7 * 24 * 60 * 60  # 7 dias
        cookie['session']['httponly'] = True
        cookie['session']['samesite'] = 'Lax'
        sys.stdout.write(f"{cookie.output()}\r\n")
    
    sys.stdout.write("\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()
