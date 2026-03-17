<?php
/**
 * form_list.php — Lista dados salvos em SQLite
 * GET /cgi-bin/form_list.php
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

    $params = [];
    $method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
    if (!empty($_GET)) {
        $params = array_merge($params, $_GET);
    }
    if (!empty($_POST)) {
        $params = array_merge($params, $_POST);
    }
    if ($method === 'POST') {
        $cl = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
        if ($cl > 0) {
            $stdin = fopen('php://stdin', 'r');
            $raw = fread($stdin, $cl);
            fclose($stdin);
            $body_params = [];
            parse_str($raw, $body_params);
            if (!empty($body_params)) {
                $params = array_merge($params, $body_params);
            }
        }
    } else if (!empty($_SERVER['QUERY_STRING'])) {
        $qs_params = [];
        parse_str($_SERVER['QUERY_STRING'], $qs_params);
        if (!empty($qs_params)) {
            $params = array_merge($params, $qs_params);
        }
    }

    $action = $params['action'] ?? 'list';

    if ($action === 'delete') {
        $id = (int)($params['id'] ?? 0);
        if ($id > 0) {
            $stmt = $db->prepare("DELETE FROM entries WHERE id = ?");
            $stmt->bindValue(1, $id, SQLITE3_INTEGER);
            $stmt->execute();
            $result = ["ok" => true, "message" => "Entrada deletada"];
        } else {
            $result = ["ok" => false, "error" => "ID inválido"];
        }
    } else {
        // list
        $stmt = $db->prepare(
            "SELECT id, name, email, message, created_at FROM entries ORDER BY created_at DESC LIMIT 50"
        );
        $res = $stmt->execute();
        $entries = [];
        while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
            $entries[] = $row;
        }
        $result = ["ok" => true, "entries" => $entries];
    }

    echo json_encode($result, JSON_UNESCAPED_UNICODE);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(["ok" => false, "error" => $e->getMessage()], JSON_UNESCAPED_UNICODE);
}
