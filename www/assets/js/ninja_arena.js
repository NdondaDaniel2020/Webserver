
/* ════════════════════════════════════════════════════
   State
   ════════════════════════════════════════════════════ */
const CGI_PY = '/cgi-bin/arena_action.py';
const CGI_PHP = '/cgi-bin/arena_enemy.php';
const COOKIE_NAME = 'NAGSID';
const CLASSES_META = {
    shadow: { emoji: '🥷', sp_name: 'Shadow Step', sp_cost: 40 },
    blade: { emoji: '⚔️', sp_name: 'Blade Storm', sp_cost: 45 },
    monk: { emoji: '☯️', sp_name: 'Iron Body', sp_cost: 30 },
};

let selectedClass = 'shadow';
let isBusy = false;
let lastPlayer = null;
let lastBattle = null;
let lastEnemy = null;
let comboCount = 0;
let comboTimer = null;

/* ════════════════════════════════════════════════════
   Cookie helpers
   ════════════════════════════════════════════════════ */
function getSid() {
    const m = document.cookie.match(/(?:^|;\s*)NAGSID=([^;]+)/);
    return m ? m[1] : null;
}
function setSid(sid) {
    const maxAge = 24 * 60 * 60; // 24 horas
    document.cookie = `NAGSID=${encodeURIComponent(sid)}; path=/; max-age=${maxAge}; SameSite=Lax`;
    localStorage.setItem('NAGSID_BACKUP', sid);
}

/* ════════════════════════════════════════════════════
   API calls
   ════════════════════════════════════════════════════ */
async function callCgi(url, params) {
    // Se o cookie não existe, tenta usar localStorage
    if (!getSid() && localStorage.getItem('NAGSID_BACKUP')) {
        setSid(localStorage.getItem('NAGSID_BACKUP'));
    }

    const body = new URLSearchParams(params).toString();
    const resp = await fetch(url, {
        method: 'POST',
        credentials: 'include',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
    });
    const data = await resp.json();
    if (data.sid) setSid(data.sid);
    return data;
}

/* ════════════════════════════════════════════════════
   Setup screen + Arena Creation
   ════════════════════════════════════════════════════ */
function pickClass(btn, cls) {
    document.querySelectorAll('.class-btn').forEach(b => b.classList.remove('picked'));
    btn.classList.add('picked');
    selectedClass = cls;
}

async function initializeArena() {
    const btn = document.getElementById('btn-arena-create');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> AGUARDE...';

    // Simulate arena initialization (can add backend call if needed)
    setTimeout(() => {
        btn.disabled = false;
        btn.innerHTML = '⚡ CRIAR ARENA';
        showCharacterCreation();
    }, 500);
}

async function createCharacter() {
    const name = document.getElementById('inp-name').value.trim() || 'Ninja';
    document.getElementById('btn-create').disabled = true;
    document.getElementById('btn-create').innerHTML = '<span class="spinner"></span> AGUARDE...';

    const data = await callCgi(CGI_PY, { action: 'new_game', name, class: selectedClass });
    if (!data.ok) { showToast('Erro: ' + data.error, true); resetCreateBtn(); return; }

    if (data.sid) setSid(data.sid);

    // Start first battle
    const b = await callCgi(CGI_PY, { action: 'start_battle' });
    if (!b.ok) { showToast('Erro: ' + b.error, true); resetCreateBtn(); return; }

    transitionToBattle(b.player, b.battle, b.enemy);
}

function resetCreateBtn() {
    const btn = document.getElementById('btn-create');
    btn.disabled = false;
    btn.innerHTML = '▶ ENTRAR NA ARENA';
}

async function loadScoreboard() {
    try {
        const data = await callCgi(CGI_PY, { action: 'scoreboard' });
        const el = document.getElementById('scoreboard-list');
        if (!data.ok || !data.scores.length) {
            el.innerHTML = '<span style="color:var(--text-muted);font-size:.8rem">Sem recordes ainda. Seja o primeiro!</span>';
            return;
        }
        el.innerHTML = data.scores.map((s, i) =>
            `<div class="score-row">
        <span><strong style="color:var(--yellow)">#${i + 1}</strong>
          <span class="score-name"> ${esc(s.name)}</span>
          <span style="color:var(--text-muted)"> (${s.class})</span></span>
        <span class="score-stats">LV${s.level} · ${s.wins}W · 🪙${s.gold}</span>
      </div>`
        ).join('');
    } catch (e) { /* no-op */ }
}

/* ════════════════════════════════════════════════════
   Screen transitions
   ════════════════════════════════════════════════════ */
function showScreen(id) {
    document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
    document.getElementById(id).classList.add('active');
}

function showSetup() {
    showScreen('arena-creation-screen');
}

function showCharacterCreation() {
    showScreen('setup-screen');
    document.getElementById('btn-create').disabled = false;
    document.getElementById('btn-create').innerHTML = '▶ ENTRAR NA ARENA';
    loadScoreboard();
}

function transitionToBattle(player, battle, enemy) {
    lastPlayer = player;
    lastBattle = battle;
    lastEnemy = enemy;
    resetCombo(); // Reset combo counter
    setDefendingState(true, false); // Clear defending states
    setDefendingState(false, false);
    updateBattleUI(player, battle, enemy);
    showScreen('battle-screen');
    setActionsBusy(false);
}

/* ════════════════════════════════════════════════════
   Sprite Generation
   ════════════════════════════════════════════════════ */
function getNinjaSprite(className) {
    const sprites = {
        shadow: `
      <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
        <!-- Shadow Ninja - Purple theme -->
        <g id="shadow-ninja">
          <!-- Body --><ellipse cx="100" cy="180" rx="30" ry="8" fill="rgba(0,0,0,0.3)"/>
          <rect x="85" y="100" width="30" height="50" rx="5" fill="#2D1B4E"/>
          <rect x="80" y="140" width="15" height="35" rx="3" fill="#2D1B4E"/>
          <rect x="105" y="140" width="15" height="35" rx="3" fill="#2D1B4E"/>
          <!-- Arms -->
          <rect x="70" y="105" width="12" height="40" rx="3" fill="#2D1B4E" transform="rotate(-25 76 105)"/>
          <rect x="118" y="105" width="12" height="40" rx="3" fill="#2D1B4E" transform="rotate(25 124 105)"/>
          <!-- Head -->
          <circle cx="100" cy="80" r="22" fill="#3D2B5E"/>
          <rect x="80" y="75" width="40" height="8" fill="#1a1a2e" rx="2"/>
          <!-- Eyes -->
          <circle cx="92" cy="78" r="3" fill="#A855F7"/>
          <circle cx="108" cy="78" r="3" fill="#A855F7"/>
          <!-- Katana -->
          <rect x="55" y="120" width="4" height="45" fill="#4A5568" transform="rotate(-45 57 120)"/>
          <rect x="53" y="115" width="8" height="8" fill="#8B4513" rx="2"/>
          <line x1="55" y1="165" x2="58" y2="162" stroke="#E0E0E0" stroke-width="2"/>
        </g>
      </svg>`,

        blade: `
      <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
        <!-- Blade Ninja - Cyan theme -->
        <g id="blade-ninja">
          <ellipse cx="100" cy="180" rx="30" ry="8" fill="rgba(0,0,0,0.3)"/>
          <rect x="85" y="100" width="30" height="50" rx="5" fill="#0D3D56"/>
          <rect x="80" y="140" width="15" height="35" rx="3" fill="#0D3D56"/>
          <rect x="105" y="140" width="15" height="35" rx="3" fill="#0D3D56"/>
          <!-- Arms with dual swords -->
          <rect x="65" y="105" width="12" height="40" rx="3" fill="#0D3D56" transform="rotate(-35 71 105)"/>
          <rect x="123" y="105" width="12" height="40" rx="3" fill="#0D3D56" transform="rotate(35 129 105)"/>
          <!-- Head -->
          <circle cx="100" cy="80" r="22" fill="#1A5266"/>
          <rect x="80" y="75" width="40" height="8" fill="#0A2533" rx="2"/>
          <circle cx="92" cy="78" r="3" fill="#06B6D4"/>
          <circle cx="108" cy="78" r="3" fill="#06B6D4"/>
          <!-- Dual Katanas -->
          <rect x="50" y="100" width="3" height="50" fill="#C0C0C0" transform="rotate(-50 52 100)"/>
          <rect x="147" y="100" width="3" height="50" fill="#C0C0C0" transform="rotate(50 149 100)"/>
          <path d="M 52 95 L 56 95 L 55 105 L 53 105 Z" fill="#06B6D4"/>
          <path d="M 147 95 L 151 95 L 150 105 L 148 105 Z" fill="#06B6D4"/>
        </g>
      </svg>`,

        monk: `
      <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
        <!-- Monk Ninja - Orange theme -->
        <g id="monk-ninja">
          <ellipse cx="100" cy="180" rx="30" ry="8" fill="rgba(0,0,0,0.3)"/>
          <rect x="85" y="100" width="30" height="50" rx="5" fill="#8B4513"/>
          <rect x="80" y="140" width="15" height="35" rx="3" fill="#8B4513"/>
          <rect x="105" y="140" width="15" height="35" rx="3" fill="#8B4513"/>
          <!-- Arms in martial arts pose -->
          <rect x="70" y="90" width="12" height="45" rx="3" fill="#8B4513" transform="rotate(-15 76 90)"/>
          <rect x="118" y="90" width="12" height="45" rx="3" fill="#8B4513" transform="rotate(15 124 90)"/>
          <!-- Head with headband -->
          <circle cx="100" cy="80" r="22" fill="#A0522D"/>
          <rect x="78" y="70" width="44"height="6" fill="#F97316" rx="2"/>
          <circle cx="92" cy="78" r="3" fill="#FF6B35"/>
          <circle cx="108" cy="78" r="3" fill="#FF6B35"/>
          <!-- Energy glow -->
          <circle cx="100" cy="115" r="8" fill="#F97316" opacity="0.3"/>
        </g>
      </svg>`
    };

    return sprites[className] || sprites.shadow;
}

function getEnemySprite(enemyName) {
    const lowername = (enemyName || '').toLowerCase();

    // Different enemy designs based on name
    if (lowername.includes('ronin') || lowername.includes('samurai')) {
        return `
      <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
        <g id="ronin">
          <ellipse cx="100" cy="180" rx="30" ry="8" fill="rgba(0,0,0,0.3)"/>
          <rect x="85" y="100" width="30" height="50" rx="5" fill="#8B0000"/>
          <rect x="80" y="140" width="15" height="35" rx="3" fill="#8B0000"/>
          <rect x="105" y="140" width="15" height="35" rx="3" fill="#8B0000"/>
          <!-- Armor plates -->
          <rect x="83" y="105" width="34" height="8" fill="#B8860B"/>
          <rect x="83" y="120" width="34" height="8" fill="#B8860B"/>
          <!-- Arms -->
          <rect x="70" y="105" width="12" height="45" rx="3" fill="#8B0000"/>
          <rect x="118" y="105" width="12" height="45" rx="3" fill="#8B0000"/>
          <!-- Head -->
          <circle cx="100" cy="80" r="22" fill="#A52A2A"/>
          <rect x="78" y="65" width="44" height="12" fill="#B8860B" rx="2"/>
          <circle cx="92" cy="82" r="3" fill="#EF4444"/>
          <circle cx="108" cy="82" r="3" fill="#EF4444"/>
          <!-- Large Katana -->
          <rect x="125" y="90" width="5" height="65" fill="#696969" transform="rotate(35 127 90)"/>
          <rect x="123" y="85" width="9" height="10" fill="#DAA520" rx="2"/>
        </g>
      </svg>`;
    }

    if (lowername.includes('oni') || lowername.includes('demon')) {
        return `
      <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
        <g id="oni">
          <ellipse cx="100" cy="185" rx="35" ry="10" fill="rgba(0,0,0,0.4)"/>
          <!-- Large body -->
          <rect x="80" y="100" width="40" height="60" rx="8" fill="#4A0E0E"/>
          <rect x="75" y="145" width="20" height="40" rx="4" fill="#4A0E0E"/>
          <rect x="105" y="145" width="20" height="40" rx="4" fill="#4A0E0E"/>
          <!-- Muscular arms -->
          <rect x="60" y="100" width="18" height="50" rx="4" fill="#5A1E1E"/>
          <rect x="122" y="100" width="18" height="50" rx="4" fill="#5A1E1E"/>
          <!-- Large head -->
          <circle cx="100" cy="75" r="28" fill="#6A1E1E"/>
          <!-- Horns -->
          <path d="M 75 60 Q 70 45 72 50 Q 74 55 77 58" fill="#2C1810" stroke="#2C1810" stroke-width="3"/>
          <path d="M 125 60 Q 130 45 128 50 Q 126 55 123 58" fill="#2C1810" stroke="#2C1810" stroke-width="3"/>
          <!-- Fierce eyes -->
          <circle cx="90" cy="75" r="4" fill="#FF0000"/>
          <circle cx="110" cy="75" r="4" fill="#FF0000"/>
          <!-- Club weapon -->
          <rect x="135" y="100" width="12" height="55" rx="6" fill="#654321" transform="rotate(25 141 100)"/>
          <circle cx="143" cy="145" r="8" fill="#654321"/>
        </g>
      </svg>`;
    }

    // Default enemy
    return `
    <svg viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
      <g id="enemy-thug">
        <ellipse cx="100" cy="180" rx="30" ry="8" fill="rgba(0,0,0,0.3)"/>
        <rect x="85" y="105" width="30" height="45" rx="5" fill="#3D3D3D"/>
        <rect x="80" y="140" width="15" height="35" rx="3" fill="#3D3D3D"/>
        <rect x="105" y="140" width="15" height="35" rx="3" fill="#3D3D3D"/>
        <rect x="72" y="110" width="12" height="38" rx="3" fill="#3D3D3D" transform="rotate(-20 78 110)"/>
        <rect x="116" y="110" width="12" height="38" rx="3" fill="#3D3D3D" transform="rotate(20 122 110)"/>
        <circle cx="100" cy="85" r="20" fill="#4D4D4D"/>
        <rect x="82" y="80" width="36" height="6" fill="#2D2D2D" rx="2"/>
        <circle cx="93" cy="85" r="2" fill="#EF4444"/>
        <circle cx="107" cy="85" r="2" fill="#EF4444"/>
      </g>
    </svg>`;
}

function showAttackEffect(attackType) {
    const effect = document.getElementById('attack-effect');
    const effects = {
        attack: '⚔️',
        special: '✨',
        defend: '🛡️',
        critical: '💥'
    };

    effect.textContent = effects[attackType] || '💢';
    effect.style.animation = 'none';
    setTimeout(() => {
        effect.style.animation = 'attackFlash 0.6s ease-out';
    }, 10);
}

function shakeEnemy() {
    const enemySprite = document.getElementById('enemy-sprite');
    enemySprite.classList.add('hit-shake');
    setTimeout(() => enemySprite.classList.remove('hit-shake'), 300);
}

function shakePlayer() {
    const playerSprite = document.getElementById('player-sprite');
    playerSprite.classList.add('hit-shake', 'player');
    setTimeout(() => {
        playerSprite.classList.remove('hit-shake', 'player');
    }, 300);
}

/* ════════════════════════════════════════════════════
   Enhanced Battle Animations
   ════════════════════════════════════════════════════ */
function playAttackAnimation(isPlayer) {
    const sprite = document.getElementById(isPlayer ? 'player-sprite' : 'enemy-sprite');
    sprite.classList.add('attacking');
    setTimeout(() => sprite.classList.remove('attacking'), 600);
}

function showCritical() {
    const critText = document.getElementById('critical-text');
    critText.style.display = 'block';
    critText.style.animation = 'none';
    setTimeout(() => {
        critText.style.animation = 'criticalPop 1s ease-out forwards';
    }, 10);
    setTimeout(() => {
        critText.style.display = 'none';
    }, 1000);

    // Add glow to enemy
    const enemySprite = document.getElementById('enemy-sprite');
    enemySprite.classList.add('critical-glow');
    setTimeout(() => enemySprite.classList.remove('critical-glow'), 400);
}

function updateCombo(hit) {
    if (hit) {
        comboCount++;
        const comboEl = document.getElementById('combo-counter');
        const comboNumEl = document.getElementById('combo-num');

        if (comboCount >= 2) {
            comboEl.classList.add('show');
            comboNumEl.textContent = comboCount;

            // Clear previous timer
            clearTimeout(comboTimer);

            // Reset combo after 3 seconds of no hits
            comboTimer = setTimeout(() => {
                resetCombo();
            }, 3000);
        }
    } else {
        resetCombo();
    }
}

function resetCombo() {
    comboCount = 0;
    const comboEl = document.getElementById('combo-counter');
    comboEl.classList.remove('show');
    setTimeout(() => {
        document.getElementById('combo-num').textContent = '0';
    }, 300);
}

function setDefendingState(isPlayer, active) {
    const sprite = document.getElementById(isPlayer ? 'player-sprite' : 'enemy-sprite');
    if (active) {
        sprite.classList.add('defending');
    } else {
        sprite.classList.remove('defending');
    }
}

async function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

/* ════════════════════════════════════════════════════
   Battle UI updates
   ════════════════════════════════════════════════════ */
function updateBattleUI(player, battle, enemy) {
    if (!player || !battle || !enemy) return;
    const cls = CLASSES_META[player.class] || CLASSES_META.shadow;

    // Update sprites
    document.getElementById('player-sprite').innerHTML = getNinjaSprite(player.class);
    document.getElementById('enemy-sprite').innerHTML = getEnemySprite(enemy.name);

    // Player
    document.getElementById('p-emoji').textContent = cls.emoji;
    document.getElementById('p-name').textContent = player.name;
    document.getElementById('p-class-badge').textContent = player.class.toUpperCase();
    document.getElementById('p-atk').textContent = player.atk;
    document.getElementById('p-def').textContent = player.def_;
    document.getElementById('p-xp').textContent = player.xp;
    document.getElementById('p-gold').textContent = player.gold;
    document.getElementById('p-wins').textContent = player.wins;
    document.getElementById('p-sp-name').textContent = cls.sp_name;
    document.getElementById('p-sp-cost').textContent = cls.sp_cost;
    document.getElementById('btn-sp-cost').textContent = cls.sp_cost;
    document.getElementById('level-info').textContent = `LV ${player.level} · ${player.xp} XP`;

    setHpBar('p-hp-bar', 'p-hp-text', player.hp, player.max_hp, false);
    setEnergyBar(player.energy, player.max_energy);

    // Enemy
    document.getElementById('e-emoji').textContent = enemy.emoji;
    document.getElementById('e-name').textContent = enemy.name;
    document.getElementById('e-atk').textContent = enemy.atk;
    document.getElementById('e-def').textContent = enemy.def_;
    document.getElementById('e-level-badge').textContent = `WAVE ${player.level}`;
    setHpBar('e-hp-bar', 'e-hp-text', battle.enemy_hp, battle.enemy_max_hp, true);

    // Threat descriptor
    const hpPct = battle.enemy_hp / battle.enemy_max_hp;
    document.getElementById('e-threat').textContent =
        hpPct < 0.25 ? '⚠️ MODO DESESPERO — perigo máximo!' :
            hpPct < 0.5 ? '🔥 Inimigo enfraquecido — finalize!' :
                hpPct < 0.75 ? '😤 Inimigo irritado' :
                    '😠 Inimigo em plena forma';

    // Round / Phase
    document.getElementById('round-tag').textContent = `ROUND ${battle.round}`;
    const phase = battle.phase;
    const phaseEl = document.getElementById('phase-indicator');
    if (phase === 'player_turn') {
        phaseEl.textContent = 'SEU TURNO';
        phaseEl.className = 'phase-indicator';
        document.getElementById('player-card').classList.add('active-turn');
        document.getElementById('enemy-card').classList.remove('active-turn');
    } else if (phase === 'enemy_turn' || phase === 'resolve') {
        phaseEl.textContent = 'TURNO INIMIGO';
        phaseEl.className = 'phase-indicator enemy';
        document.getElementById('enemy-card').classList.add('active-turn');
        document.getElementById('player-card').classList.remove('active-turn');
    } else {
        phaseEl.textContent = 'AGUARDANDO';
        phaseEl.className = 'phase-indicator wait';
    }

    // Special button energy check
    document.getElementById('btn-special').disabled =
        player.energy < cls.sp_cost || phase !== 'player_turn';

    // Render log
    renderLog(battle.log);
}

function setHpBar(barId, textId, hp, max, isEnemy) {
    const pct = max > 0 ? Math.max(0, (hp / max) * 100) : 0;
    const bar = document.getElementById(barId);
    bar.style.width = pct + '%';
    bar.classList.remove('low', 'med');
    if (!isEnemy) {
        if (pct < 25) bar.classList.add('low');
        else if (pct < 50) bar.classList.add('med');
    } else {
        if (pct < 25) bar.classList.add('low');
    }
    document.getElementById(textId).textContent = `${hp}/${max}`;
}

function setEnergyBar(energy, max) {
    const pct = max > 0 ? Math.max(0, (energy / max) * 100) : 0;
    document.getElementById('p-en-bar').style.width = pct + '%';
    document.getElementById('p-en-text').textContent = `${energy}/${max}`;
}

function renderLog(logJson) {
    let entries;
    try { entries = typeof logJson === 'string' ? JSON.parse(logJson) : logJson; }
    catch { entries = []; }
    const el = document.getElementById('combat-log');
    el.innerHTML = entries.map(e => {
        // Detect critical hits in message
        const isCritical = (e.msg || '').toLowerCase().includes('crítico') ||
            (e.msg || '').toLowerCase().includes('critical');

        let cls = e.actor === 'player' ? 'entry-player' :
            e.actor === 'enemy' ? 'entry-enemy' : 'entry-system';

        if (isCritical) {
            cls = 'entry-critical';
        }

        // Add icons based on action
        let icon = '';
        if (e.msg.includes('ataca')) icon = '⚔️ ';
        if (e.msg.includes('habilidade') || e.msg.includes('especial')) icon = '✨ ';
        if (e.msg.includes('defende') || e.msg.includes('bloqueia')) icon = '🛡️ ';
        if (isCritical) icon = '💥 ';
        if (e.msg.includes('fugiu') || e.msg.includes('escapou')) icon = '🏃 ';

        return `<div class="${cls}">${icon}${esc(e.msg)}</div>`;
    }).join('');
    el.scrollTop = el.scrollHeight;
}

function setActionsBusy(busy) {
    isBusy = busy;
    ['btn-attack', 'btn-special', 'btn-defend', 'btn-flee'].forEach(id => {
        document.getElementById(id).disabled = busy;
    });
    if (!busy && lastPlayer && lastBattle) {
        const cls = CLASSES_META[lastPlayer.class] || CLASSES_META.shadow;
        document.getElementById('btn-special').disabled =
            lastPlayer.energy < cls.sp_cost || lastBattle.phase !== 'player_turn';
    }
}

/* ════════════════════════════════════════════════════
   Action dispatch
   ════════════════════════════════════════════════════ */
async function doAction(action) {
    if (isBusy) return;
    setActionsBusy(true);

    const phaseEl = document.getElementById('phase-indicator');
    phaseEl.textContent = 'PROCESSANDO...';
    phaseEl.className = 'phase-indicator wait';

    // Player attack animation
    if (action === 'attack' || action === 'special') {
        playAttackAnimation(true);
        await sleep(300); // Wait for attack to land
    }

    // Set defending state
    if (action === 'defend') {
        setDefendingState(true, true);
    }

    let data;
    try {
        data = await callCgi(CGI_PY, { action });
    } catch (e) {
        showToast('Erro de rede. Verifique o servidor.', true);
        setActionsBusy(false);
        return;
    }

    if (!data.ok) {
        showToast(data.error || 'Erro desconhecido', true);
        setActionsBusy(false);
        if (lastPlayer && lastBattle && lastEnemy)
            updateBattleUI(lastPlayer, lastBattle, lastEnemy);
        return;
    }

    // Update UI with Python's response
    if (data.player) lastPlayer = data.player;
    if (data.battle) lastBattle = data.battle;
    if (data.enemy) lastEnemy = data.enemy;

    // Show damage splash on enemy card
    if (data.hit || data.sp_dmg) {
        const dmg = data.hit || data.sp_dmg;
        const isCritical = data.critical || false;

        showDmgSplash('enemy-card', 'enemy-hit', '-' + dmg);
        shakeEl('enemy-card');
        shakeEnemy();
        showAttackEffect(data.sp_dmg ? 'special' : 'attack');

        // Critical hit
        if (isCritical) {
            await sleep(200);
            showCritical();
            updateCombo(true);
        } else if (data.hit) {
            updateCombo(true);
        }

        await sleep(400); // Delay after hit for impact feel
    } else if (action === 'attack') {
        // Missed attack
        updateCombo(false);
    }

    // Check outcome
    if (data.action === 'victory' || data.action === 'defeat' ||
        (data.battle && (data.battle.status === 'victory' || data.battle.status === 'defeat')) ||
        data.action === 'flee' && data.fled) {
        handleBattleEnd(data);
        return;
    }

    updateBattleUI(lastPlayer, lastBattle, lastEnemy);

    await sleep(300); // Delay before enemy turn

    // If it's now enemy turn, call PHP
    if (data.next === 'php_enemy_turn') {
        await doEnemyTurn();
    } else {
        setActionsBusy(false);
    }
}

async function doEnemyTurn() {
    await sleep(500); // Dramatic pause before enemy attacks

    // Enemy attack animation
    playAttackAnimation(false);
    await sleep(300);

    let eData;
    try {
        eData = await callCgi(CGI_PHP, { action: 'enemy_turn' });
    } catch (e) {
        showToast('Erro no turno do inimigo.', true);
        setActionsBusy(false);
        return;
    }

    if (!eData.ok) {
        showToast(eData.error || 'Erro do inimigo', true);
        setActionsBusy(false);
        return;
    }

    // Damage flash on player card
    if (eData.dmg_dealt > 0) {
        showDmgSplash('player-card', 'player-hit', '-' + eData.dmg_dealt);
        shakeEl('player-card');
        shakePlayer();
        showAttackEffect('attack');
        await sleep(400); // Impact delay
    } else {
        showDmgSplash('player-card', 'player-hit', '0');
        await sleep(200);
    }

    // Update state from PHP response
    if (eData.player) lastPlayer = eData.player;
    if (eData.battle) lastBattle = eData.battle;
    if (eData.enemy) lastEnemy = eData.enemy;

    updateBattleUI(lastPlayer, lastBattle, lastEnemy);

    await sleep(300); // Delay before resolve

    // Python resolves the round
    if (eData.next === 'py_resolve') {
        let rData;
        try {
            rData = await callCgi(CGI_PY, { action: 'resolve_enemy' });
        } catch (e) {
            showToast('Erro ao resolver round.', true);
            setActionsBusy(false);
            return;
        }

        if (!rData.ok) {
            // Could be defeat resolved
            if (rData.action === 'defeat' || (rData.battle && rData.battle.status === 'defeat')) {
                handleBattleEnd(rData);
                return;
            }
            showToast(rData.error || 'Erro na resolução', true);
            setActionsBusy(false);
            return;
        }

        if (rData.player) lastPlayer = rData.player;
        if (rData.battle) lastBattle = rData.battle;
        if (rData.enemy) lastEnemy = rData.enemy;

        if (rData.action === 'defeat' || (rData.battle && rData.battle.status === 'defeat')) {
            handleBattleEnd(rData);
            return;
        }

        updateBattleUI(lastPlayer, lastBattle, lastEnemy);
        setActionsBusy(false);
    } else {
        setActionsBusy(false);
    }
}

/* ════════════════════════════════════════════════════
   Battle end
   ════════════════════════════════════════════════════ */
function handleBattleEnd(data) {
    const isVic = data.action === 'victory' ||
        (data.battle && data.battle.status === 'victory');
    const isFled = data.action === 'flee' && data.fled;

    if (data.player) lastPlayer = data.player;
    if (data.battle) lastBattle = data.battle;
    if (data.enemy) lastEnemy = data.enemy;

    const card = document.getElementById('result-card');
    const title = document.getElementById('result-title');
    const sub = document.getElementById('result-sub');
    const emo = document.getElementById('result-emoji');
    const rwds = document.getElementById('reward-list');

    if (isFled) {
        emo.textContent = '🏃';
        title.textContent = 'FUGIU';
        title.className = 'result-title';
        title.style.color = 'var(--yellow)';
        sub.textContent = 'Você escapou... por agora.';
        rwds.innerHTML = `<li><span>Ouro perdido</span><span class="rval" style="color:var(--red)">-10 🪙</span></li>`;
    } else if (isVic) {
        emo.textContent = '🏆';
        title.textContent = 'VITÓRIA!';
        title.className = 'result-title victory';
        title.style.color = '';
        sub.textContent = `${lastEnemy ? lastEnemy.emoji + ' ' + lastEnemy.name : 'Inimigo'} foi destruído!`;
        rwds.innerHTML = `
      <li><span>XP ganho</span><span class="rval">+${data.xp_gained || '?'} XP</span></li>
      <li><span>Ouro ganho</span><span class="rval">+${data.gold_gained || '?'} 🪙</span></li>
      <li><span>Nível atual</span><span class="rval">LV ${lastPlayer?.level || 1}</span></li>
      <li><span>Vitórias</span><span class="rval">${lastPlayer?.wins || 0}</span></li>`;
        if (data.leveled_up) showToast('⬆️ LEVEL UP! Força aumentada!');
    } else {
        emo.textContent = '💀';
        title.textContent = 'DERROTA';
        title.className = 'result-title defeat';
        title.style.color = '';
        sub.textContent = 'Você caiu em batalha. Revive com HP parcial.';
        rwds.innerHTML = `
      <li><span>HP de recuperação</span><span class="rval">${lastPlayer?.hp || '?'} HP</span></li>
      <li><span>Derrotas</span><span class="rval">${lastPlayer?.losses || 0}</span></li>`;
    }

    showScreen('result-screen');
}

async function nextBattle() {
    showScreen('battle-screen');
    setActionsBusy(true);
    document.getElementById('phase-indicator').textContent = 'INICIANDO...';

    const data = await callCgi(CGI_PY, { action: 'start_battle' });
    if (!data.ok) { showToast(data.error, true); showSetup(); return; }

    transitionToBattle(data.player, data.battle, data.enemy);
}

/* ════════════════════════════════════════════════════
   Visual helpers
   ════════════════════════════════════════════════════ */
function showDmgSplash(cardId, cls, text) {
    const card = document.getElementById(cardId);
    const el = document.createElement('div');
    el.className = 'dmg-splash ' + cls;
    el.textContent = text;
    card.appendChild(el);
    setTimeout(() => el.remove(), 950);
}

function shakeEl(id) {
    const el = document.getElementById(id);
    el.classList.remove('shake');
    void el.offsetWidth;
    el.classList.add('shake');
    setTimeout(() => el.classList.remove('shake'), 450);
}

let toastTimer;
function showToast(msg, isErr) {
    const el = document.getElementById('toast');
    el.textContent = msg;
    el.style.borderColor = isErr ? 'var(--red)' : 'var(--neon)';
    el.style.color = isErr ? 'var(--red)' : 'var(--neon)';
    el.style.background = isErr ? 'rgba(239,68,68,.15)' : 'rgba(0,255,156,.15)';
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.remove('show'), 3200);
}

function esc(str) {
    return String(str)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

/* ════════════════════════════════════════════════════
   Init
   ════════════════════════════════════════════════════ */
(async function init() {
    loadScoreboard();

    // Check if there's already a session with an active battle
    const sid = getSid();
    if (!sid) return;

    try {
        const data = await callCgi(CGI_PY, { action: 'state' });
        if (data.ok && data.player && data.battle && data.battle.status === 'active') {
            transitionToBattle(data.player, data.battle, data.enemy);
        }
    } catch (e) { /* fresh start */ }
})();