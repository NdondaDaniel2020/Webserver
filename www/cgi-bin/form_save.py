#!/usr/bin/env python3
"""
form_save.py — Salva dados de formulário em SQLite
POST /cgi-bin/form_save.py?name=...&email=...&message=...
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
    # Parse query string ou POST body
    qs_str = os.environ.get("QUERY_STRING", "")
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    params = parse_qs(qs_str, keep_blank_values=True)
    
    if method == "POST":
        try:
            cl = int(os.environ.get("CONTENT_LENGTH", "0"))
        except ValueError:
            cl = 0
        if cl > 0:
            body_params = parse_qs(sys.stdin.read(cl), keep_blank_values=True)
            params.update(body_params)
    
    init_db()
    
    action = params.get("action", ["list"])[0]
    
    if action == "save":
        name = params.get("name", [""])[0].strip()
        email = params.get("email", [""])[0].strip()
        message = params.get("message", [""])[0].strip()
        
        if not name or not email:
            result = {"ok": False, "error": "Name e Email são obrigatórios"}
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
            except Exception as e:
                result = {"ok": False, "error": str(e)}
    else:  # list
        try:
            conn = sqlite3.connect(DB_PATH)
            conn.row_factory = sqlite3.Row
            rows = conn.execute(
                "SELECT id, name, email, message, created_at FROM entries ORDER BY created_at DESC LIMIT 50"
            ).fetchall()
            conn.close()
            result = {
                "ok": True,
                "entries": [dict(row) for row in rows]
            }
        except Exception as e:
            result = {"ok": False, "error": str(e)}
    
    # Output
    sys.stdout.write("Content-Type: application/json\r\n\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()
