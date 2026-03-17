#!/usr/bin/env python3
"""
arena_action.py  — Python game engine for Ninja Arena
Actions: new_game, attack, defend, special, next_round
Flow: Python (action) -> PHP (enemy turn) -> Python (resolve)
DB: SQLite at /tmp/ninja_arena/game.db
"""

import json
import os
import re
import sqlite3
import sys
import time
from urllib.parse import parse_qs

DB_DIR  = "/tmp/ninja_arena"
DB_PATH = DB_DIR + "/game.db"
COOKIE_NAME = "NAGSID"
SID_RE = re.compile(r"^[A-Za-z0-9]{32,64}$")

# ── Ninja classes ────────────────────────────────────
CLASSES = {
    "shadow": {"name": "Shadow",  "emoji": "🥷", "hp": 120, "atk": 22, "def_": 8,  "sp_name": "Shadow Step",   "sp_dmg": 45, "sp_cost": 40},
    "blade":  {"name": "Blade",   "emoji": "⚔️",  "hp": 100, "atk": 28, "def_": 5,  "sp_name": "Blade Storm",   "sp_dmg": 55, "sp_cost": 45},
    "monk":   {"name": "Monk",    "emoji": "☯️",  "hp": 140, "atk": 18, "def_": 14, "sp_name": "Iron Body",     "sp_dmg": 35, "sp_cost": 30},
}

# ── Enemy pool per difficulty wave ──────────────────
ENEMIES = [
    {"name": "Ronin",      "emoji": "👺", "hp": 80,  "atk": 16, "def_": 4,  "reward_xp": 30,  "reward_gold": 15},
    {"name": "Oni",        "emoji": "👹", "hp": 110, "atk": 22, "def_": 6,  "reward_xp": 55,  "reward_gold": 28},
    {"name": "Serpent",    "emoji": "🐍", "hp": 90,  "atk": 26, "def_": 3,  "reward_xp": 50,  "reward_gold": 25},
    {"name": "Stone Golem","emoji": "🗿", "hp": 160, "atk": 18, "def_": 14, "reward_xp": 80,  "reward_gold": 45},
    {"name": "Shadow Lord","emoji": "💀", "hp": 200, "atk": 32, "def_": 10, "reward_xp": 150, "reward_gold": 80},
]


def _import_secrets():
    import secrets
    return secrets

# ─────────────────────────────────────────────────────
def get_db():
    os.makedirs(DB_DIR, exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    _create_schema(conn)
    return conn


def _create_schema(conn):
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS players (
            sid         TEXT PRIMARY KEY,
            name        TEXT NOT NULL DEFAULT 'Ninja',
            class       TEXT NOT NULL DEFAULT 'shadow',
            hp          INTEGER NOT NULL DEFAULT 120,
            max_hp      INTEGER NOT NULL DEFAULT 120,
            energy      INTEGER NOT NULL DEFAULT 100,
            max_energy  INTEGER NOT NULL DEFAULT 100,
            atk         INTEGER NOT NULL DEFAULT 22,
            def_        INTEGER NOT NULL DEFAULT 8,
            level       INTEGER NOT NULL DEFAULT 1,
            xp          INTEGER NOT NULL DEFAULT 0,
            gold        INTEGER NOT NULL DEFAULT 0,
            wins        INTEGER NOT NULL DEFAULT 0,
            losses      INTEGER NOT NULL DEFAULT 0,
            created_at  INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS battles (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            player_sid    TEXT NOT NULL,
            enemy_idx     INTEGER NOT NULL DEFAULT 0,
            enemy_hp      INTEGER NOT NULL DEFAULT 0,
            enemy_max_hp  INTEGER NOT NULL DEFAULT 80,
            round         INTEGER NOT NULL DEFAULT 1,
            phase         TEXT NOT NULL DEFAULT 'player_turn',
            status        TEXT NOT NULL DEFAULT 'active',
            defending     INTEGER NOT NULL DEFAULT 0,
            log           TEXT NOT NULL DEFAULT '[]',
            started_at    INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS scoreboard (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            class      TEXT NOT NULL,
            level      INTEGER NOT NULL DEFAULT 1,
            wins       INTEGER NOT NULL DEFAULT 0,
            gold       INTEGER NOT NULL DEFAULT 0,
            recorded   INTEGER NOT NULL DEFAULT 0
        );
    """)
    conn.commit()


# ─────────────────────────────────────────────────────
def parse_cookies(raw):
    cookies = {}
    for part in raw.split(";"):
        part = part.strip()
        if "=" in part:
            k, v = part.split("=", 1)
            cookies[k.strip()] = v.strip()
    return cookies


def sid_from_cookie(raw):
    cookies = parse_cookies(raw)
    sid = cookies.get(COOKIE_NAME, "")
    if SID_RE.match(sid):
        return sid, False
    sec = _import_secrets()
    return sec.token_hex(16), True


def get_player(conn, sid):
    row = conn.execute("SELECT * FROM players WHERE sid=?", (sid,)).fetchone()
    return dict(row) if row else None


def get_active_battle(conn, sid):
    row = conn.execute(
        "SELECT * FROM battles WHERE player_sid=? AND status='active' ORDER BY id DESC LIMIT 1",
        (sid,)
    ).fetchone()
    return dict(row) if row else None


def log_append(log_json, msg, actor="system"):
    entries = json.loads(log_json)
    entries.append({"t": int(time.time()), "actor": actor, "msg": msg})
    return json.dumps(entries[-30:])


def level_threshold(level):
    return level * level * 60


def try_level_up(conn, player):
    changed = False
    while player["xp"] >= level_threshold(player["level"]):
        player["xp"] -= level_threshold(player["level"])
        player["level"] += 1
        player["max_hp"]     += 15
        player["max_energy"] += 10
        player["atk"]        += 3
        player["def_"]       += 1
        player["hp"]          = player["max_hp"]
        player["energy"]      = player["max_energy"]
        changed = True
    if changed:
        conn.execute(
            "UPDATE players SET level=?, xp=?, max_hp=?, max_energy=?, atk=?, def_=?, hp=?, energy=? WHERE sid=?",
            (player["level"], player["xp"], player["max_hp"], player["max_energy"],
             player["atk"], player["def_"], player["hp"], player["energy"], player["sid"])
        )
        conn.commit()
    return changed, player


# ─────────────────────────── ACTIONS ────────────────

def action_new_game(conn, sid, params):
    name  = params.get("name",  ["Ninja"])[0][:20].strip() or "Ninja"
    cls   = params.get("class", ["shadow"])[0].lower()
    if cls not in CLASSES:
        cls = "shadow"

    c = CLASSES[cls]
    now = int(time.time())

    existing = get_player(conn, sid)
    if existing:
        conn.execute("DELETE FROM battles WHERE player_sid=?", (sid,))
        conn.execute("DELETE FROM players WHERE sid=?", (sid,))

    conn.execute("""
        INSERT INTO players (sid,name,class,hp,max_hp,energy,max_energy,atk,def_,level,xp,gold,wins,losses,created_at)
        VALUES (?,?,?,?,?,100,100,?,?,1,0,0,0,0,?)
    """, (sid, name, cls, c["hp"], c["hp"], c["atk"], c["def_"], now))
    conn.commit()

    return {"ok": True, "action": "new_game", "player": get_player(conn, sid)}


def action_start_battle(conn, sid):
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Cria personagem primeiro."}

    existing = get_active_battle(conn, sid)
    if existing:
        enemy = ENEMIES[existing["enemy_idx"]]
        return {"ok": True, "action": "start_battle", "battle": existing,
                "enemy": enemy, "player": player}

    # Pick enemy scaled to level
    idx = min(player["level"] - 1, len(ENEMIES) - 1)
    enemy = ENEMIES[idx]
    now   = int(time.time())

    conn.execute("""
        INSERT INTO battles (player_sid,enemy_idx,enemy_hp,enemy_max_hp,round,phase,status,defending,log,started_at)
        VALUES (?,?,?,?,1,'player_turn','active',0,?,?)
    """, (sid, idx, enemy["hp"], enemy["hp"], json.dumps([
        {"t": now, "actor": "system",
         "msg": f"⚔️ Batalha iniciada! {player['name']} vs {enemy['emoji']} {enemy['name']}"}
    ]), now))
    conn.commit()

    battle = get_active_battle(conn, sid)
    return {"ok": True, "action": "start_battle", "battle": battle,
            "enemy": enemy, "player": player}


def action_attack(conn, sid):
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Sem personagem."}
    battle = get_active_battle(conn, sid)
    if not battle or battle["phase"] != "player_turn":
        return {"ok": False, "error": "Não é o seu turno."}

    import random
    enemy = ENEMIES[battle["enemy_idx"]]

    # Critical chance 15%
    crit = random.random() < 0.15
    base_dmg = max(1, player["atk"] - enemy["def_"] + random.randint(-3, 3))
    dmg = int(base_dmg * 1.8) if crit else base_dmg

    new_enemy_hp = max(0, battle["enemy_hp"] - dmg)
    crit_txt = " 💥 CRÍTICO!" if crit else ""
    log_msg = f"🗡️ {player['name']} ataca {enemy['name']} por {dmg} dano!{crit_txt}"
    new_log = log_append(battle["log"], log_msg, "player")

    if new_enemy_hp <= 0:
        return _resolve_victory(conn, sid, player, battle, enemy, new_log)

    # Advance to enemy turn
    conn.execute("UPDATE battles SET enemy_hp=?, phase='enemy_turn', defending=0, log=? WHERE id=?",
                 (new_enemy_hp, new_log, battle["id"]))
    conn.commit()

    battle = get_active_battle(conn, sid)
    return {"ok": True, "action": "attack", "hit": dmg, "crit": crit,
            "battle": battle, "enemy": enemy, "player": player, "phase": "enemy_turn",
            "next": "php_enemy_turn"}


def action_defend(conn, sid):
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Sem personagem."}
    battle = get_active_battle(conn, sid)
    if not battle or battle["phase"] != "player_turn":
        return {"ok": False, "error": "Não é o seu turno."}

    energy_gain = 20
    new_energy = min(player["max_energy"], player["energy"] + energy_gain)
    conn.execute("UPDATE players SET energy=? WHERE sid=?", (new_energy, sid))

    log_msg = f"🛡️ {player['name']} defende! +{energy_gain} energia."
    new_log = log_append(battle["log"], log_msg, "player")
    conn.execute("UPDATE battles SET phase='enemy_turn', defending=1, log=? WHERE id=?",
                 (new_log, battle["id"]))
    conn.commit()

    player  = get_player(conn, sid)
    battle  = get_active_battle(conn, sid)
    enemy   = ENEMIES[battle["enemy_idx"]]
    return {"ok": True, "action": "defend",
            "battle": battle, "enemy": enemy, "player": player,
            "phase": "enemy_turn", "next": "php_enemy_turn"}


def action_special(conn, sid):
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Sem personagem."}
    battle = get_active_battle(conn, sid)
    if not battle or battle["phase"] != "player_turn":
        return {"ok": False, "error": "Não é o seu turno."}

    cls_data = CLASSES[player["class"]]
    sp_cost  = cls_data["sp_cost"]
    if player["energy"] < sp_cost:
        return {"ok": False, "error": f"Energia insuficiente! Precisa de {sp_cost}."}

    import random
    enemy   = ENEMIES[battle["enemy_idx"]]
    sp_dmg  = cls_data["sp_dmg"] + random.randint(-5, 5)
    new_ep  = max(0, battle["enemy_hp"] - sp_dmg)
    new_energy = player["energy"] - sp_cost

    conn.execute("UPDATE players SET energy=? WHERE sid=?", (new_energy, sid))

    log_msg = f"✨ {player['name']} usa {cls_data['sp_name']}! {sp_dmg} dano devastador!"
    new_log = log_append(battle["log"], log_msg, "player")

    if new_ep <= 0:
        conn.execute("UPDATE players SET energy=? WHERE sid=?", (new_energy, sid))
        conn.commit()
        player = get_player(conn, sid)
        return _resolve_victory(conn, sid, player, battle, enemy, new_log)

    conn.execute("UPDATE battles SET enemy_hp=?, phase='enemy_turn', defending=0, log=? WHERE id=?",
                 (new_ep, new_log, battle["id"]))
    conn.commit()

    battle = get_active_battle(conn, sid)
    player = get_player(conn, sid)
    return {"ok": True, "action": "special", "sp_dmg": sp_dmg,
            "battle": battle, "enemy": enemy, "player": player,
            "phase": "enemy_turn", "next": "php_enemy_turn"}


def action_resolve_enemy(conn, sid):
    """Called after PHP enemy turn to let Python close the round."""
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Sem personagem."}
    battle = get_active_battle(conn, sid)
    if not battle:
        return {"ok": False, "error": "Sem batalha ativa."}
    if battle["phase"] != "resolve":
        return {"ok": False, "error": "Fase incorreta."}

    enemy = ENEMIES[battle["enemy_idx"]]

    if player["hp"] <= 0:
        return _resolve_defeat(conn, sid, player, battle, enemy)

    # New round
    new_round = battle["round"] + 1
    log_msg   = f"🔄 Round {new_round} começa! Faça sua escolha."
    new_log   = log_append(battle["log"], log_msg, "system")
    conn.execute("UPDATE battles SET round=?, phase='player_turn', log=? WHERE id=?",
                 (new_round, new_log, battle["id"]))
    conn.commit()

    battle = get_active_battle(conn, sid)
    return {"ok": True, "action": "resolve_enemy",
            "battle": battle, "enemy": enemy, "player": player, "phase": "player_turn"}


def action_flee(conn, sid):
    player = get_player(conn, sid)
    if not player:
        return {"ok": False, "error": "Sem personagem."}
    battle = get_active_battle(conn, sid)
    if not battle:
        return {"ok": False, "error": "Sem batalha ativa."}

    import random
    enemy = ENEMIES[battle["enemy_idx"]]
    if random.random() < 0.5:
        log_msg = "🏃 Fugiu com sucesso! (perde 10 ouro)"
        new_log = log_append(battle["log"], log_msg, "system")
        new_gold = max(0, player["gold"] - 10)
        conn.execute("UPDATE players SET gold=? WHERE sid=?", (new_gold, sid))
        conn.execute("UPDATE battles SET status='fled', log=? WHERE id=?", (new_log, battle["id"]))
        conn.commit()
        player = get_player(conn, sid)
        return {"ok": True, "action": "flee", "fled": True, "player": player}
    else:
        log_msg = "🔒 Fuga bloqueada pelo inimigo!"
        new_log = log_append(battle["log"], log_msg, "system")
        conn.execute("UPDATE battles SET phase='enemy_turn', log=? WHERE id=?",
                     (new_log, battle["id"]))
        conn.commit()
        battle = get_active_battle(conn, sid)
        return {"ok": True, "action": "flee", "fled": False,
                "battle": battle, "enemy": enemy, "player": player,
                "phase": "enemy_turn", "next": "php_enemy_turn"}


def action_scoreboard(conn):
    rows = conn.execute(
        "SELECT name,class,level,wins,gold FROM scoreboard ORDER BY wins DESC, gold DESC LIMIT 10"
    ).fetchall()
    return {"ok": True, "action": "scoreboard",
            "scores": [dict(r) for r in rows]}


# ─────────────────────────── HELPERS ────────────────

def _resolve_victory(conn, sid, player, battle, enemy, log_json):
    xp_gain   = enemy["reward_xp"]
    gold_gain = enemy["reward_gold"]
    new_xp    = player["xp"]   + xp_gain
    new_gold  = player["gold"] + gold_gain
    new_wins  = player["wins"] + 1
    # Restore 30% HP on victory
    new_hp    = min(player["max_hp"], player["hp"] + int(player["max_hp"] * 0.3))
    new_energy = min(player["max_energy"], player["energy"] + 25)

    log_json = log_append(log_json,
        f"🏆 {player['name']} venceu! +{xp_gain} XP  +{gold_gain} 🪙 ouro", "system")

    conn.execute("UPDATE battles SET enemy_hp=0, status='victory', phase='done', log=? WHERE id=?",
                 (log_json, battle["id"]))
    conn.execute("""UPDATE players SET hp=?, energy=?, xp=?, gold=?, wins=? WHERE sid=?""",
                 (new_hp, new_energy, new_xp, new_gold, new_wins, sid))
    conn.commit()

    player = dict(conn.execute("SELECT * FROM players WHERE sid=?", (sid,)).fetchone())
    leveled, player = try_level_up(conn, player)

    # Record scoreboard
    conn.execute("INSERT INTO scoreboard (name,class,level,wins,gold,recorded) VALUES (?,?,?,?,?,?)",
                 (player["name"], player["class"], player["level"], player["wins"], player["gold"], int(time.time())))
    conn.commit()

    return {"ok": True, "action": "victory", "xp_gained": xp_gain,
            "gold_gained": gold_gain, "leveled_up": leveled,
            "player": player, "battle": dict(conn.execute("SELECT * FROM battles WHERE id=?", (battle["id"],)).fetchone())}


def _resolve_defeat(conn, sid, player, battle, enemy):
    log_json = log_append(battle["log"],
        f"💀 {player['name']} foi derrotado por {enemy['emoji']} {enemy['name']}!", "system")
    new_losses = player["losses"] + 1
    revive_hp  = player["max_hp"] // 3

    conn.execute("UPDATE battles SET status='defeat', phase='done', log=? WHERE id=?",
                 (log_json, battle["id"]))
    conn.execute("UPDATE players SET hp=?, energy=100, losses=? WHERE sid=?",
                 (revive_hp, new_losses, sid))
    conn.commit()

    player = get_player(conn, sid)
    return {"ok": True, "action": "defeat", "player": player,
            "battle": dict(conn.execute("SELECT * FROM battles WHERE id=?", (battle["id"],)).fetchone())}


# ─────────────────────────── MAIN ───────────────────

def main():
    raw_cookie = os.environ.get("HTTP_COOKIE", "")
    sid, new_sid = sid_from_cookie(raw_cookie)

    qs = parse_qs(os.environ.get("QUERY_STRING", ""), keep_blank_values=True)
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    if method == "POST":
        try:
            cl = int(os.environ.get("CONTENT_LENGTH", "0"))
        except ValueError:
            cl = 0
        if cl > 0:
            body_params = parse_qs(sys.stdin.read(cl), keep_blank_values=True)
            qs.update(body_params)

    act = qs.get("action", ["state"])[0]

    conn = get_db()

    if act == "new_game":
        result = action_new_game(conn, sid, qs)
    elif act == "start_battle":
        result = action_start_battle(conn, sid)
    elif act == "attack":
        result = action_attack(conn, sid)
    elif act == "defend":
        result = action_defend(conn, sid)
    elif act == "special":
        result = action_special(conn, sid)
    elif act == "resolve_enemy":
        result = action_resolve_enemy(conn, sid)
    elif act == "flee":
        result = action_flee(conn, sid)
    elif act == "scoreboard":
        result = action_scoreboard(conn)
    else:  # state
        player = get_player(conn, sid)
        battle = get_active_battle(conn, sid) if player else None
        enemy  = ENEMIES[battle["enemy_idx"]] if battle else None
        result = {"ok": True, "action": "state", "player": player,
                  "battle": battle, "enemy": enemy}

    conn.close()

    headers = ["Content-Type: application/json"]
    if new_sid:
        headers.append(f"Set-Cookie: {COOKIE_NAME}={sid}; Path=/; HttpOnly; SameSite=Lax")
        result["sid"] = sid
    body = json.dumps(result, ensure_ascii=False)
    sys.stdout.write("\r\n".join(headers) + "\r\n\r\n" + body)


if __name__ == "__main__":
    main()
