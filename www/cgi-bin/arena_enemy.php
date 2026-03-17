<?php
/**
 * arena_enemy.php — PHP enemy AI turn for Ninja Arena
 * Called after player action with phase='enemy_turn'
 * Reads SQLite DB, decides enemy action, applies damage, returns JSON
 * DB: /tmp/ninja_arena/game.db
 */

define('DB_PATH',    '/tmp/ninja_arena/game.db');
define('COOKIE_NAME','NAGSID');

// ── Enemy pool (must mirror arena_action.py) ────────
const ENEMIES = [
    ['name'=>'Ronin',       'emoji'=>'👺','hp'=>80,  'atk'=>16,'def_'=>4,  'reward_xp'=>30, 'reward_gold'=>15],
    ['name'=>'Oni',         'emoji'=>'👹','hp'=>110, 'atk'=>22,'def_'=>6,  'reward_xp'=>55, 'reward_gold'=>28],
    ['name'=>'Serpent',     'emoji'=>'🐍','hp'=>90,  'atk'=>26,'def_'=>3,  'reward_xp'=>50, 'reward_gold'=>25],
    ['name'=>'Stone Golem', 'emoji'=>'🗿','hp'=>160, 'atk'=>18,'def_'=>14, 'reward_xp'=>80, 'reward_gold'=>45],
    ['name'=>'Shadow Lord', 'emoji'=>'💀','hp'=>200, 'atk'=>32,'def_'=>10, 'reward_xp'=>150,'reward_gold'=>80],
];

// ── Helpers ──────────────────────────────────────────

function get_sid(): array {
    $raw = $_SERVER['HTTP_COOKIE'] ?? '';
    $cookies = [];
    foreach (explode(';', $raw) as $part) {
        $part = trim($part);
        if (strpos($part, '=') !== false) {
            [$k, $v] = explode('=', $part, 2);
            $cookies[trim($k)] = trim($v);
        }
    }
    $sid = $cookies[COOKIE_NAME] ?? '';
    if (preg_match('/^[A-Za-z0-9]{32,64}$/', $sid)) {
        return [$sid, false];
    }
    $new = bin2hex(random_bytes(16));
    return [$new, true];
}

function get_db(): SQLite3 {
    if (!file_exists(DB_PATH)) {
        json_error(500, 'Banco de dados não encontrado. Crie um personagem primeiro.');
    }
    return new SQLite3(DB_PATH);
}

function get_player(SQLite3 $db, string $sid): ?array {
    $stmt = $db->prepare('SELECT * FROM players WHERE sid=?');
    $stmt->bindValue(1, $sid, SQLITE3_TEXT);
    $row = $stmt->execute()->fetchArray(SQLITE3_ASSOC);
    return $row ?: null;
}

function get_active_battle(SQLite3 $db, string $sid): ?array {
    $stmt = $db->prepare(
        "SELECT * FROM battles WHERE player_sid=? AND status='active' ORDER BY id DESC LIMIT 1"
    );
    $stmt->bindValue(1, $sid, SQLITE3_TEXT);
    $row = $stmt->execute()->fetchArray(SQLITE3_ASSOC);
    return $row ?: null;
}

function log_append(string $log_json, string $msg, string $actor = 'enemy'): string {
    $entries = json_decode($log_json, true) ?: [];
    $entries[] = ['t' => time(), 'actor' => $actor, 'msg' => $msg];
    return json_encode(array_slice($entries, -30));
}

function json_error(int $code, string $msg): void {
    header('Content-Type: application/json');
    echo json_encode(['ok' => false, 'error' => $msg], JSON_UNESCAPED_UNICODE);
    exit;
}

// ── Enemy AI logic ───────────────────────────────────

function enemy_decide(array $enemy, array $battle, array $player): array {
    $hp_pct = $battle['enemy_hp'] / max(1, $battle['enemy_max_hp']);
    $r      = mt_rand(0, 99);

    if ($hp_pct < 0.25) {
        // Desperate: 40% chance special double-attack
        if ($r < 40) return ['type' => 'special'];
    }

    if ($hp_pct < 0.5) {
        // Bloodied: aggressive
        if ($r < 60) return ['type' => 'attack'];
        if ($r < 80) return ['type' => 'counter'];
        return ['type' => 'taunt'];
    }

    // Normal: random
    if ($r < 50) return  ['type' => 'attack'];
    if ($r < 70) return  ['type' => 'counter'];
    if ($r < 85) return  ['type' => 'taunt'];
    return                ['type' => 'attack'];
}

function apply_enemy_action(
    SQLite3 $db, array $enemy, array $battle, array $player,
    array $decision, string $sid
): array {
    $action_type = $decision['type'];
    $defending   = (int)$battle['defending'];
    $new_log     = $battle['log'];
    $dmg         = 0;
    $log_msg     = '';
    $effect      = '';

    $base_atk = $enemy['atk'];

    switch ($action_type) {
        case 'special':
            $dmg     = (int)($base_atk * 1.7) + mt_rand(-4, 4);
            if ($defending) $dmg = (int)($dmg * 0.45);
            $dmg     = max(1, $dmg - $player['def_']);
            $log_msg = "💢 {$enemy['emoji']} {$enemy['name']} usa ATAQUE DEVASTADOR! {$dmg} dano!";
            $effect  = 'special';
            break;

        case 'counter':
            $dmg     = (int)($base_atk * 0.8) + mt_rand(-2, 2);
            if ($defending) $dmg = (int)($dmg * 0.3);
            $dmg     = max(1, $dmg - $player['def_']);
            $log_msg = "🔄 {$enemy['emoji']} {$enemy['name']} contra-ataca! {$dmg} dano.";
            $effect  = 'counter';
            break;

        case 'taunt':
            $dmg     = 0;
            $log_msg = "😤 {$enemy['emoji']} {$enemy['name']} provoca você! (+0 dano)";
            $effect  = 'taunt';
            break;

        default: // attack
            $dmg     = $base_atk + mt_rand(-3, 3);
            if ($defending) $dmg = (int)($dmg * 0.4);
            $dmg     = max(1, $dmg - $player['def_']);
            $log_msg = "⚔️ {$enemy['emoji']} {$enemy['name']} ataca {$player['name']}! {$dmg} dano.";
            if ($defending) $log_msg .= " 🛡️ Defendido!";
            $effect  = 'attack';
            break;
    }

    $new_log  = log_append($new_log, $log_msg, 'enemy');
    $new_hp   = max(0, $player['hp'] - $dmg);

    // Save player HP
    $stmt = $db->prepare('UPDATE players SET hp=? WHERE sid=?');
    $stmt->bindValue(1, $new_hp, SQLITE3_INTEGER);
    $stmt->bindValue(2, $sid,    SQLITE3_TEXT);
    $stmt->execute();

    // Advance battle phase to 'resolve'
    $stmt2 = $db->prepare('UPDATE battles SET phase=?, log=? WHERE id=?');
    $stmt2->bindValue(1, 'resolve',         SQLITE3_TEXT);
    $stmt2->bindValue(2, $new_log,           SQLITE3_TEXT);
    $stmt2->bindValue(3, $battle['id'],      SQLITE3_INTEGER);
    $stmt2->execute();

    return [
        'ok'          => true,
        'action'      => 'enemy_turn',
        'enemy_action'=> $action_type,
        'effect'      => $effect,
        'dmg_dealt'   => $dmg,
        'player_hp'   => $new_hp,
        'defending'   => $defending,
        'phase'       => 'resolve',
        'next'        => 'py_resolve',
        'log_msg'     => $log_msg,
    ];
}

// ── Main ─────────────────────────────────────────────

[$sid, $new_sid] = get_sid();

$params = [];
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $cl = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($cl > 0) {
        $stdin = fopen('php://stdin', 'r');
        $raw   = fread($stdin, $cl);
        fclose($stdin);
        parse_str($raw, $params);
    }
}

// Validate action
$action = $params['action'] ?? 'enemy_turn';
if ($action !== 'enemy_turn') {
    json_error(400, 'Ação inválida para arena_enemy.php');
}

$db     = get_db();
$player = get_player($db, $sid);
if (!$player) {
    json_error(404, 'Personagem não encontrado. Crie um personagem primeiro.');
}

$battle = get_active_battle($db, $sid);
if (!$battle) {
    json_error(404, 'Sem batalha ativa.');
}
if ($battle['phase'] !== 'enemy_turn') {
    json_error(400, "Fase incorreta: '{$battle['phase']}'. Esperado: enemy_turn.");
}

$enemy    = ENEMIES[$battle['enemy_idx']];
$decision = enemy_decide($enemy, $battle, $player);
$result   = apply_enemy_action($db, $enemy, $battle, $player, $decision, $sid);
$db->close();

// Re-read updated player for response
$db2    = get_db();
$player = get_player($db2, $sid);
$battle_r = get_active_battle($db2, $sid);
$db2->close();

$result['player'] = $player;
$result['battle'] = $battle_r;
$result['enemy']  = $enemy;

header('Content-Type: application/json');
if ($new_sid) {
    header('Set-Cookie: ' . COOKIE_NAME . "={$sid}; Path=/; HttpOnly; SameSite=Lax");
}
echo json_encode($result, JSON_UNESCAPED_UNICODE);
