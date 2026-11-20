# units.json に場面のメモを付け、text/chunks/cNNN.json（id, en, note）に分ける
import os, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
R = [(0x44635c, 0x4463f8, 'monster type name (internal list; rarely shown)'),
     (0x446ab8, 0x446b6c, 'spell book page heading'),
     (0x446b70, 0x446ba0, 'in-game menu (Esc)'),
     (0x4477a8, 0x4477d0, 'default player-character name (shown on status screen); a playful name'),
     (0x4486c0, 0x4486e0, 'message line (bottom of screen)'),
     (0x44a79c, 0x44a970, 'level list of the cheat/debug "View Level" menu: number - level name'),
     (0x44a974, 0x44a9d0, 'cheat menu item; followed by " on." / " off." when toggled'),
     (0x44aa00, 0x44aa60, 'character preset / class / stat label'),
     (0x44aa60, 0x44aab0, 'status/inventory label (very narrow, keep short)'),
     (0x44aab0, 0x44ab40, 'options / title menu item'),
     (0x44ab48, 0x44ad70, 'message or yes/no prompt shown in a small box; some prompts are split over 2 lines (two consecutive entries)'),
     (0x44ad74, 0x44ada0, 'difficulty'),
     (0x44ada4, 0x44aedc, 'status text / message line (%s = character, monster, spell or item name)'),
     (0x44aedc, 0x44b140, 'spell name (also used in "%s expired." etc.)'),
     (0x44b9c8, 0x44bb30, 'level (dungeon area) name'),
     (0x44c3c8, 0x44c440, 'automap / debug view legend'),
     (0x44e200, 0x44e2c0, 'key configuration list: action name (keep short)'),
     (0x44e2e0, 0x44e310, 'key configuration list: key name abbreviation (UpAr = up arrow, Spac = space bar, Shft = shift)'),
     (0x45cf78, 0x45cfc8, 'options menu item'),
     (0x45d098, 0x45d470, 'system dialog / message box text'),
     (0x45d92c, 0x45e290, 'item name (inventory, "%s picked up")'),
     (0x463768, 0x4639b0, 'projectile / spell effect name (internal; rarely shown)'),
     (0x4789ac, 0x478a20, 'prompt / status label'),
     (0x478ca8, 0x478d90, 'system message / message line')]
SPECIAL = {
 'Do you now wish to attempt to': 'LINE 1 of a 2-line yes/no prompt; line 2 is "save the current game?" (asked before quitting)',
 'save the current game?': 'LINE 2 of a 2-line yes/no prompt; line 1 is "Do you now wish to attempt to"',
 'Are you sure you wish to start': 'LINE 1 of a 2-line yes/no prompt; line 2 is " this level all over again?" (Restart Level)',
 ' this level all over again?': 'LINE 2 of a 2-line yes/no prompt; line 1 is "Are you sure you wish to start"',
 's killed: ': 'end-of-level statistics, drawn right after a monster name ("Goblin" + "s killed: " + number). Translate as " 撃破数: "',
 'Lvl:  XP:': 'status bar label: level and experience (numbers drawn after each label)',
}
units = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
for u in units:
    if u['en'] in SPECIAL: u['note'] = SPECIAL[u['en']]; continue
    if u.get('note'): continue
    if u['va'] == 'extra':
        u['note'] = {' on.': 'appended after a cheat menu item when switched on', ' off.': 'appended after a cheat menu item when switched off',
                     '%s (%d)': 'item name with count'}.get(u['en'], 'short label (status, spell level ordinal or key name)')
        continue
    va = int(u['va'], 16)
    for a, b, n in R:
        if a <= va < b: u['note'] = n; break
    else: u['note'] = ''
json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
N = 4
per = (len(units) + N - 1) // N
for i in range(N):
    part = [{'id': u['id'], 'en': u['en'], 'note': u['note']} for u in units[i * per:(i + 1) * per]]
    json.dump(part, open(os.path.join(ROOT, 'text', 'chunks', 'c%03d.json' % (i + 1)), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(units), 'units in', N, 'chunks;', sum(1 for u in units if not u['note']), 'without note')
for u in units:
    if not u['note']: print(' ', u['id'], u['va'], u['en'])
