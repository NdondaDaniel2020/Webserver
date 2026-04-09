#!/usr/bin/env python3
"""
form_save.py — Salva dados de formulário em SQLite
POST /cgi-bin/form_save.py (name e email obrigatórios no body)
"""

import os
import sqlite3
import sys
import json
from urllib.parse import parse_qs

DB_PATH = "/tmp/form_data.db"

def init_db():
    """Cria tabela se não existir"""
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT NOT NULL,
            message TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()

    init_db()

    # Apenas POST é aceito
    if method != "POST":
        result = {"ok": False, "error": "Use POST para salvar dados"}
        status = "405 Method Not Allowed"
    else:
        # Parse POST body
        try:
            cl = int(os.environ.get("CONTENT_LENGTH", "0"))
        except ValueError:
            cl = 0
        
        params = {}
        if cl > 0:
            body_params = parse_qs(sys.stdin.read(cl), keep_blank_values=True)
            params.update(body_params)
        
        name = params.get("name", [""])[0].strip()
        email = params.get("email", [""])[0].strip()
        message = params.get("message", [""])[0].strip()
        
        if not name or not email:
            result = {"ok": False, "error": "name e email são obrigatórios"}
            status = "400 Bad Request"
        else:
            try:
                conn = sqlite3.connect(DB_PATH)
                conn.execute(
                    "INSERT INTO entries (name, email, message) VALUES (?, ?, ?)",
                    (name, email, message)
                )
                conn.commit()
                conn.close()
                result = {"ok": True, "message": "Dados salvos com sucesso"}
                status = "201 Created"
            except Exception as e:
                result = {"ok": False, "error": str(e)}
                status = "500 Internal Server Error"
    
    # Output HTTP headers and JSON
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()
