#!/usr/bin/env python3
"""
form_list.py — Lista/deleta dados salvos em SQLite
GET /cgi-bin/form_list.py (lista todos)
DELETE /cgi-bin/form_list.py?id=1 (deleta por ID)
POST /cgi-bin/form_list.py com _method=DELETE também funciona como fallback
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

def get_query_param(name):
    """Extrai parâmetro da query string"""
    query_string = os.environ.get("QUERY_STRING", "")
    if not query_string:
        return ""
    params = parse_qs(query_string)
    return params.get(name, [""])[0]

def list_entries():
    """GET: lista todas as entradas ordenadas por data (DESC)"""
    try:
        conn = sqlite3.connect(DB_PATH)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        cursor.execute(
            "SELECT id, name, email, message, created_at FROM entries ORDER BY created_at DESC LIMIT 50"
        )
        entries = [dict(row) for row in cursor.fetchall()]
        conn.close()
        return {"ok": True, "entries": entries}, "200 OK"
    except Exception as e:
        return {"ok": False, "error": str(e)}, "500 Internal Server Error"

def delete_entry(entry_id):
    """DELETE: deleta uma entrada por ID"""
    # Validar ID
    try:
        entry_id = int(entry_id)
    except (ValueError, TypeError):
        return {"ok": False, "error": "ID inválido"}, "400 Bad Request"
    
    if entry_id <= 0:
        return {"ok": False, "error": "ID inválido"}, "400 Bad Request"
    
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM entries WHERE id = ?", (entry_id,))
        conn.commit()
        conn.close()
        return {"ok": True, "message": "Entrada deletada com sucesso"}, "200 OK"
    except Exception as e:
        return {"ok": False, "error": str(e)}, "500 Internal Server Error"

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    
    init_db()
    
    # GET: listar todas as entradas
    if method == "GET":
        result, status = list_entries()
    
    # DELETE: deletar por ID (via query string)
    elif method == "DELETE":
        entry_id = get_query_param("id")
        result, status = delete_entry(entry_id)
    
    # POST: suporta fallback para DELETE via _method
    elif method == "POST":
        try:
            cl = int(os.environ.get("CONTENT_LENGTH", "0"))
        except ValueError:
            cl = 0
        
        body = ""
        if cl > 0:
            body = sys.stdin.read(cl)
        
        # Parse body
        params = parse_qs(body, keep_blank_values=True)
        request_method = params.get("_method", [""])[0].upper()
        
        if request_method == "DELETE":
            # Tenta obter ID do body ou query string
            entry_id = params.get("id", [""])[0] or get_query_param("id")
            result, status = delete_entry(entry_id)
        else:
            result = {"ok": False, "error": "Use GET para listar ou DELETE para remover"}
            status = "405 Method Not Allowed"
    
    else:
        result = {"ok": False, "error": "Use GET para listar ou DELETE para remover"}
        status = "405 Method Not Allowed"
    
    # Output HTTP headers and JSON
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n\r\n")
    sys.stdout.write(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()
