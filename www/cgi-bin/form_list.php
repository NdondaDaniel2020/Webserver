<?php
/**
 * form_list.php — Lista/deleta dados salvos em SQLite
 * GET /cgi-bin/form_list.php (lista todos)
 * DELETE /cgi-bin/form_list.php?id=1 (deleta por ID)
 * POST /cgi-bin/form_list.php?id=1 com method=post também funciona como fallback
 */

$db_path = "/tmp/form_data.db";

// Função para garantir que o banco existe (com schema)
function init_db() {
    global $db_path;
    $db = new SQLite3($db_path);
    $db->exec("
        CREATE TABLE IF NOT EXISTS entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT NOT NULL,
            message TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ");
    return $db;
}

header('Content-Type: application/json');

try {
    $db = init_db();
    $method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

    if ($method === 'DELETE' || (isset($_GET['_method']) && $_GET['_method'] === 'DELETE')) {
        // Deletar por ID (via query string ou request body)
        $id = (int)($_GET['id'] ?? 0);
        if ($id <= 0) {
            // Tenta ler do body
            $cl = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
            if ($cl > 0) {
                $stdin = fopen('php://stdin', 'r');
                $raw = fread($stdin, $cl);
                fclose($stdin);
                $body_params = [];
                parse_str($raw, $body_params);
                $id = (int)($body_params['id'] ?? 0);
            }
        }

        if ($id > 0) {
            $stmt = $db->prepare("DELETE FROM entries WHERE id = ?");
            $stmt->bindValue(1, $id, SQLITE3_INTEGER);
            $stmt->execute();
            $result = ["ok" => true, "message" => "Entrada deletada com sucesso"];
            http_response_code(200);
        } else {
            $result = ["ok" => false, "error" => "ID inválido"];
            http_response_code(400);
        }
    } else if ($method === 'GET') {
        // GET: listar todas as entradas
        $stmt = $db->prepare(
            "SELECT id, name, email, message, created_at FROM entries ORDER BY created_at DESC LIMIT 50"
        );
        $res = $stmt->execute();
        $entries = [];
        while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
            $entries[] = $row;
        }
        $result = ["ok" => true, "entries" => $entries];
        http_response_code(200);
    } else {
        $result = ["ok" => false, "error" => "Use GET para listar ou DELETE para remover"];
        http_response_code(405);
    }

    echo json_encode($result, JSON_UNESCAPED_UNICODE);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(["ok" => false, "error" => $e->getMessage()], JSON_UNESCAPED_UNICODE);
}

