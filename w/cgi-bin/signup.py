#!/usr/bin/env python3
"""
signup.py — Cria nova conta de utilizador com hash de password
POST /cgi-bin/signup.py (username, email, password obrigatórios)
"""

import os
import sqlite3
import sys
import json
import hashlib
import secrets
from urllib.parse import parse_qs
import re

DB_PATH = "/tmp/users.db"

def init_db():
    """Cria tabela de utilizadores se não existir"""
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

def hash_password(password):
    """Cria hash da password com salt"""
    salt = secrets.token_hex(16)
    hash_obj = hashlib.pbkdf2_hmac('sha256', password.encode(), salt.encode(), 100000)
    return f"{salt}${hash_obj.hex()}"

def is_valid_email(email):
    """Valida formato de email"""
    pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    return re.match(pattern, email) is not None

def is_valid_username(username):
    """Valida username: 3-20 caracteres, apenas alfanuméricos e underscore"""
    pattern = r'^[a-zA-Z0-9_]{3,20}$'
    return re.match(pattern, username) is not None

def signup_user(username, email, password):
    """POST: cria nova conta de utilizador"""
    
    # Validações
    if not username or not email or not password:
        return {"ok": False, "error": "username, email e password são obrigatórios"}, "400 Bad Request"
    
    username = username.strip()
    email = email.strip().lower()
    password = password.strip()
    
    if not is_valid_username(username):
        return {"ok": False, "error": "Username deve ter 3-20 caracteres (apenas letras, números e _)"}, "400 Bad Request"
    
    if not is_valid_email(email):
        return {"ok": False, "error": "Email inválido"}, "400 Bad Request"
    
    if len(password) < 6:
        return {"ok": False, "error": "Password deve ter pelo menos 6 caracteres"}, "400 Bad Request"
    
    try:
        conn = sqlite3.connect(DB_PATH)
        password_hash = hash_password(password)
        
        conn.execute(
            "INSERT INTO users (username, email, password_hash) VALUES (?, ?, ?)",
            (username, email, password_hash)
        )
        conn.commit()
        conn.close()
        
        return {"ok": True, "message": "Conta criada com sucesso"}, "201 Created"
    
    except sqlite3.IntegrityError as e:
        if "username" in str(e):
            return {"ok": False, "error": "Este username já está registado"}, "409 Conflict"
        elif "email" in str(e):
            return {"ok": False, "error": "Este email já está registado"}, "409 Conflict"
        else:
            return {"ok": False, "error": "Erro ao registar: " + str(e)}, "400 Bad Request"
    
    except Exception as e:
        return {"ok": False, "error": str(e)}, "500 Internal Server Error"

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    init_db()
    
    # Apenas POST é aceito
    if method != "POST":
        result = {"ok": False, "error": "Use POST para criar conta"}
        status = "405 Method Not Allowed"
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
        email = params.get("email", [""])[0].strip()
        password = params.get("password", [""])[0].strip()
        
        result, status = signup_user(username, email, password)
    
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
