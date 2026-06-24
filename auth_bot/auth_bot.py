import discord
from discord import app_commands
from discord.ext import commands
import sqlite3
import hashlib
import secrets
import string
import json
import threading
import time
from datetime import datetime, timedelta, timezone
from flask import Flask, request, jsonify
import os
import re
import urllib.request

script_dir = os.path.dirname(os.path.abspath(__file__))
config_path = os.path.join(script_dir, "config.json")

CONFIG = {}
if os.path.exists(config_path):
    with open(config_path) as f:
        CONFIG = json.load(f)

CONFIG["token"] = os.environ.get("DISCORD_TOKEN", CONFIG.get("token", ""))
CONFIG["webhook_url"] = os.environ.get("WEBHOOK_URL", CONFIG.get("webhook_url", ""))
CONFIG["api_port"] = int(os.environ.get("API_PORT", CONFIG.get("api_port", 5000)))

DATABASE_URL = os.environ.get("DATABASE_URL", "")
is_render = os.environ.get("RENDER", "").lower() == "true"
if DATABASE_URL:
    CONFIG["db_path"] = DATABASE_URL
elif is_render and os.path.isdir("/data"):
    CONFIG["db_path"] = "/data/auth.db"
elif is_render:
    CONFIG["db_path"] = os.path.join(script_dir, "auth.db")
else:
    CONFIG["db_path"] = os.environ.get("DB_PATH", CONFIG.get("db_path", os.path.join(script_dir, "auth.db")))

admin_ids_env = os.environ.get("ADMIN_IDS", "")
if admin_ids_env:
    CONFIG["admin_ids"] = [int(x.strip()) for x in admin_ids_env.split(",") if x.strip()]
elif "admin_ids" not in CONFIG:
    CONFIG["admin_ids"] = []

admins_env = os.environ.get("ADMINS", "")
if admins_env:
    CONFIG["admins"] = json.loads(admins_env)
elif "admins" not in CONFIG:
    CONFIG["admins"] = {}

CONFIG.setdefault("pix_key", "")
CONFIG.setdefault("pix_value", "25.00")
CONFIG.setdefault("pix_name", "Satella")
CONFIG.setdefault("ticket_category_id", 0)
CONFIG.setdefault("sales_channel_id", 0)
CONFIG.setdefault("welcome_channel_id", 0)
CONFIG.setdefault("welcome_role_id", 0)
CONFIG.setdefault("log_channel_id", 0)
CONFIG.setdefault("client_role_id", 0)

if os.environ.get("PIX_KEY"):
    CONFIG["pix_key"] = os.environ["PIX_KEY"]
if os.environ.get("PIX_NAME"):
    CONFIG["pix_name"] = os.environ["PIX_NAME"]
if os.environ.get("TICKET_CATEGORY_ID"):
    CONFIG["ticket_category_id"] = int(os.environ["TICKET_CATEGORY_ID"])
if os.environ.get("GUILD_ID"):
    CONFIG["guild_id"] = int(os.environ["GUILD_ID"])
if os.environ.get("WELCOME_CHANNEL_ID"):
    CONFIG["welcome_channel_id"] = int(os.environ["WELCOME_CHANNEL_ID"])
if os.environ.get("WELCOME_ROLE_ID"):
    CONFIG["welcome_role_id"] = int(os.environ["WELCOME_ROLE_ID"])
if os.environ.get("LOG_CHANNEL_ID"):
    CONFIG["log_channel_id"] = int(os.environ["LOG_CHANNEL_ID"])
if os.environ.get("CLIENT_ROLE_ID"):
    CONFIG["client_role_id"] = int(os.environ["CLIENT_ROLE_ID"])

def gerar_pix_brcode(chave: str, valor: float, nome: str, cidade: str = "Sao Paulo", txid: str = "***") -> str:
    def crc16(s: str) -> str:
        crc = 0xFFFF
        for c in s.encode():
            crc ^= c
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0x8408
                else:
                    crc >>= 1
        return format(crc, "04X").upper()
    def add(tag: int, val: str) -> str:
        return f"{tag:02d}{len(val):02d}{val}"
    gui = "BR.GOV.BCB.PIX"
    valor_fmt = f"{valor:.2f}"
    nome_fmt = nome.upper()[:25]
    cidade_fmt = cidade.upper()[:15]
    payload = (add(0, "01") + add(1, "12") + add(26, add(0, gui) + add(1, chave))
        + add(52, "0000") + add(53, "986") + add(54, valor_fmt)
        + add(58, "BR") + add(59, nome_fmt) + add(60, cidade_fmt)
        + add(62, add(5, txid)))
    return payload + "6304" + crc16(payload + "6304")

DB = None
lock = threading.Lock()
using_pg = False
PSYCOPG2_AVAILABLE = False

try:
    import psycopg2
    from psycopg2.extras import RealDictCursor
    PSYCOPG2_AVAILABLE = True
except ImportError:
    pass

def get_db():
    global DB, using_pg
    if DB is not None:
        return DB
    db_path = CONFIG["db_path"]
    if db_path and db_path.startswith("postgres") and PSYCOPG2_AVAILABLE:
        try:
            DB = psycopg2.connect(db_path, cursor_factory=RealDictCursor)
            DB.autocommit = True
            using_pg = True
            print("[DB] Conectado ao PostgreSQL!")
            return DB
        except Exception as e:
            print(f"[DB] PostgreSQL connection failed: {e}")
            using_pg = False
    using_pg = False
    DB = sqlite3.connect(db_path, check_same_thread=False)
    DB.row_factory = sqlite3.Row
    DB.execute("PRAGMA journal_mode=WAL")
    DB.execute("PRAGMA foreign_keys=ON")
    return DB

def db_exec(sql, params=None):
    db = get_db()
    if using_pg:
        sql = sql.replace("?", "%s")
        sql = re.sub(r'INTEGER PRIMARY KEY AUTOINCREMENT', 'SERIAL PRIMARY KEY', sql, flags=re.IGNORECASE)
        if sql.strip().upper().startswith("ALTER"):
            try:
                with db.cursor() as cur:
                    cur.execute(sql.replace("?", "%s") if params else sql, params or ())
                return True
            except:
                return False
        try:
            with db.cursor() as cur:
                cur.execute(sql, params or ())
                try:
                    rows = cur.fetchall()
                    return [dict(r) for r in rows]
                except:
                    return []
        except Exception as e:
            print(f"[DB] PG error: {e}")
            return []
    else:
        try:
            if params:
                return db.execute(sql, params)
            return db.execute(sql)
        except Exception as e:
            print(f"[DB] SQLite error: {e}")
            raise

def db_execone(sql, params=None):
    rows = db_exec(sql, params)
    if using_pg:
        return rows[0] if rows else None
    else:
        cur = rows if params is None else rows
        return cur.fetchone()

def db_execall(sql, params=None):
    rows = db_exec(sql, params)
    if using_pg:
        return rows
    else:
        cur = rows if params is None else rows
        return cur.fetchall()

def db_commit():
    if not using_pg:
        get_db().commit()

def db_lastid():
    if using_pg:
        return None
    return get_db().execute("SELECT last_insert_rowid()").fetchone()[0]

def convert_row(r):
    if using_pg:
        return r
    return dict(r) if r else None

def normalize_val(r, key, default=None):
    if r is None:
        return default
    if using_pg:
        return r.get(key, default)
    return r[key] if key in r.keys() else default

def init_db():
    db = get_db()
    if using_pg:
        db_exec("CREATE TABLE IF NOT EXISTS users (id SERIAL PRIMARY KEY, username TEXT UNIQUE NOT NULL, password_hash TEXT NOT NULL, hwid TEXT DEFAULT '', created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, banned INTEGER DEFAULT 0)")
        db_exec("CREATE TABLE IF NOT EXISTS keys (id SERIAL PRIMARY KEY, key TEXT UNIQUE NOT NULL, duration_days INTEGER NOT NULL, used_by TEXT DEFAULT NULL, used_at INTEGER DEFAULT NULL, created_at INTEGER NOT NULL)")
        db_exec("CREATE TABLE IF NOT EXISTS logs (id SERIAL PRIMARY KEY, action TEXT NOT NULL, username TEXT DEFAULT '', detail TEXT DEFAULT '', ip TEXT DEFAULT '', timestamp INTEGER NOT NULL)")
        db_exec("CREATE TABLE IF NOT EXISTS tickets (id SERIAL PRIMARY KEY, user_id INTEGER NOT NULL, channel_id INTEGER NOT NULL, ticket_type TEXT NOT NULL DEFAULT 'support', status TEXT NOT NULL DEFAULT 'open', payment_status TEXT DEFAULT 'pending', plan TEXT DEFAULT '', created_at INTEGER NOT NULL, closed_at INTEGER DEFAULT NULL)")
        db_exec("CREATE TABLE IF NOT EXISTS pix_pending (id SERIAL PRIMARY KEY, txid TEXT UNIQUE, user_id TEXT, plan TEXT, status TEXT DEFAULT 'pending', created_at INTEGER)")
        try:
            db_exec("ALTER TABLE tickets ADD COLUMN plan TEXT DEFAULT ''")
        except:
            pass
    else:
        db.executescript("""CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, password_hash TEXT NOT NULL, hwid TEXT DEFAULT '', created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, banned INTEGER DEFAULT 0);
            CREATE TABLE IF NOT EXISTS keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT UNIQUE NOT NULL, duration_days INTEGER NOT NULL, used_by TEXT DEFAULT NULL, used_at INTEGER DEFAULT NULL, created_at INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, action TEXT NOT NULL, username TEXT DEFAULT '', detail TEXT DEFAULT '', ip TEXT DEFAULT '', timestamp INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS tickets (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, channel_id INTEGER NOT NULL, ticket_type TEXT NOT NULL DEFAULT 'support', status TEXT NOT NULL DEFAULT 'open', payment_status TEXT DEFAULT 'pending', plan TEXT DEFAULT '', created_at INTEGER NOT NULL, closed_at INTEGER DEFAULT NULL);
            CREATE TABLE IF NOT EXISTS pix_pending (id INTEGER PRIMARY KEY AUTOINCREMENT, txid TEXT UNIQUE, user_id TEXT, plan TEXT, status TEXT DEFAULT 'pending', created_at INTEGER);""")
        db.commit()
        try:
            db.execute("ALTER TABLE tickets ADD COLUMN plan TEXT DEFAULT ''")
            db.commit()
        except:
            pass

def now():
    return int(time.time())

def gen_key():
    seg = lambda: ''.join(secrets.choice(string.ascii_uppercase + string.digits) for _ in range(5))
    return f"{seg()}-{seg()}-{seg()}"

def hash_pw(pw):
    return hashlib.sha256(pw.encode()).hexdigest()

def make_token():
    return secrets.token_hex(32)

tokens = {}
intents = discord.Intents.default()
intents.message_content = False
intents.members = True
bot = commands.Bot(command_prefix="!", intents=intents)

def is_admin(interaction: discord.Interaction):
    return interaction.user.id in CONFIG["admin_ids"]

class AdminGroup(app_commands.Group):
    pass

async def criar_ticket(i: discord.Interaction, tipo: str, plano: str = None):
    guild = i.guild
    user = i.user
    cat_id = CONFIG.get("ticket_category_id")
    category = guild.get_channel(cat_id) if cat_id else None
    if not category:
        category = discord.utils.get(guild.categories, name="Tickets")
    if not category:
        category = await guild.create_category("Tickets")
    existing = db_execone("SELECT id FROM tickets WHERE user_id = %s AND status = 'open' AND ticket_type = %s" if using_pg else "SELECT id FROM tickets WHERE user_id = ? AND status = 'open' AND ticket_type = ?", (str(user.id), tipo))
    if existing:
        return await i.response.send_message("Voce ja possui um ticket aberto deste tipo!", ephemeral=True)
    overwrites = {
        guild.default_role: discord.PermissionOverwrite(read_messages=False),
        user: discord.PermissionOverwrite(read_messages=True, send_messages=True, attach_files=True, read_message_history=True),
        guild.me: discord.PermissionOverwrite(read_messages=True, send_messages=True, manage_channels=True, manage_messages=True),
    }
    for uid in CONFIG.get("admin_ids", []):
        admin = guild.get_member(uid)
        if admin:
            overwrites[admin] = discord.PermissionOverwrite(read_messages=True, send_messages=True, read_message_history=True)
    prefixo = "suporte" if tipo == "suporte" else "venda"
    nome = f"{prefixo}-{user.name.lower()}"
    channel = await guild.create_text_channel(name=nome, category=category, overwrites=overwrites, topic=f"ID:{user.id}|Tipo:{tipo}|Plano:{plano or 'N/A'}")
    if using_pg:
        db_exec("INSERT INTO tickets (user_id, channel_id, ticket_type, plan, status, payment_status, created_at) VALUES (%s, %s, %s, %s, 'open', 'pending', %s)", (str(user.id), str(channel.id), tipo, plano or "", now()))
    else:
        db_exec("INSERT INTO tickets (user_id, channel_id, ticket_type, plan, status, payment_status, created_at) VALUES (?, ?, ?, ?, 'open', 'pending', ?)", (str(user.id), str(channel.id), tipo, plano or "", now()))
        db_commit()
    BANNER = "https://i.imgur.com/DSxv3VU.png"
    embed = discord.Embed(color=0xDB00A6)
    embed.set_image(url=BANNER)
    if tipo == "suporte":
        embed.title = "\U0001f4ac Suporte - Satella Private"
        embed.description = f"**Bem-vindo(a) {user.mention}!**\n\nDescreva seu problema abaixo.\nUm administrador respondera em breve.\n\n\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581\u2581"
        embed.add_field(name="\U0001f4e1 Como funciona?", value="> Envie prints, videos ou logs do erro\n> Quanto mais detalhes, mais rapido\n> Um admin respondera neste canal", inline=False)
    else:
        planos_nomes = {"diario": "Diario - R$15 (1 dia)", "semanal": "Semanal - R$50 (7 dias)", "mensal": "Mensal - R$100 (30 dias)", "lifetime": "Lifetime - R$500 (Vitalicio)"}
        planos_precos = {"diario": "15,00", "semanal": "50,00", "mensal": "100,00", "lifetime": "500,00"}
        planos_valores = {"diario": 15, "semanal": 50, "mensal": 100, "lifetime": 500}
        nome_plano = planos_nomes.get(plano, plano)
        preco = planos_precos.get(plano, CONFIG.get('pix_value', '25,00'))
        valor = planos_valores.get(plano, 25)
        pk = CONFIG.get("pix_key", "N/A")
        pn = CONFIG.get("pix_name", "Satella")
        brcode = gerar_pix_brcode(pk, valor, pn)
        qr_url = f"https://chart.googleapis.com/chart?chs=250x250&cht=qr&chl={brcode}"
        embed.title = "\U0001f4b0 Pagamento - Satella Private"
        embed.description = (f"**Bem-vindo(a) {user.mention}!**\n\n**Plano:** `{nome_plano}`\n**Valor:** R$ **{preco}**\n\n\U0001f449 **Pagamento via PIX**\n\n**Chave:** `{pk}`\n**Nome:** {pn}\n\nPague o valor exato de **R$ {preco}**\ne clique em **\"Enviar Comprovante\"** abaixo.")
        embed.set_image(url=qr_url)
    embed.set_footer(text=f"Satella Private \u2022 ID: {user.id}", icon_url=bot.user.display_avatar.url if bot.user else None)
    from views import ComprovanteView, SuporteClose
    if tipo == "venda":
        msg = await channel.send(content=user.mention, embed=embed)
        await msg.edit(view=ComprovanteView())
    else:
        msg = await channel.send(content=user.mention, embed=embed)
        await msg.edit(view=SuporteClose())
    await i.response.send_message(f"Ticket criado: {channel.mention}", ephemeral=True)

@bot.event
async def on_ready():
    print(f"[BOT] Logado como {bot.user}")
    from views import MainPanel, SalesApproveView, SalesPlanSelect, SuporteClose, SuporteConfirmClose, ComprovanteView as OldComprovanteView
    bot.add_view(MainPanel())
    bot.add_view(SalesApproveView())
    bot.add_view(OldComprovanteView())
    bot.add_view(SuporteClose())
    bot.add_view(SuporteConfirmClose())
    bot.add_view(PainelView())
    bot.add_view(PlanSelect())
    bot.add_view(ComprovanteView("", ""))
    bot.add_view(AdminApproveView("", "", 0))
    guild_id = CONFIG.get("guild_id", 0)
    try:
        if guild_id:
            guild_obj = discord.Object(id=guild_id)
            bot.tree.copy_global_to(guild=guild_obj)
            synced = await bot.tree.sync(guild=guild_obj)
            print(f"[BOT] {len(synced)} comandos sincronizados na guild {guild_id}")
        else:
            synced = await bot.tree.sync()
            print(f"[BOT] {len(synced)} comandos sincronizados globalmente")
    except Exception as e:
        print(f"[BOT] Erro sync: {e}")

@bot.event
async def on_member_join(member: discord.Member):
    guild = member.guild
    role_id = CONFIG.get("welcome_role_id", 0)
    if role_id:
        role = guild.get_role(role_id)
        if role:
            try:
                await member.add_roles(role)
                print(f"[BOT] Cargo {role.name} atribuido a {member.name}")
            except Exception as e:
                print(f"[BOT] Erro ao atribuir cargo: {e}")
    channel_id = CONFIG.get("welcome_channel_id", 0)
    if channel_id:
        channel = guild.get_channel(channel_id)
        if channel:
            embed = discord.Embed(
                title="\U0001f44b Bem-vindo!",
                color=0xDB00A6,
                description=f"**{member.mention} entrou no servidor!**\n\nBem-vindo(a) ao **Satella Private**!\nUse `/ticket` para adquirir sua licenca.",
            )
            embed.set_thumbnail(url=member.display_avatar.url)
            embed.set_footer(text="Satella Private")
            try:
                await channel.send(embed=embed)
            except Exception as e:
                print(f"[BOT] Erro ao enviar welcome: {e}")

admin_group = AdminGroup(name="admin", description="Comandos administrativos")

@admin_group.command(name="setadmin", description="Adicionar admin pelo Discord ID")
@app_commands.describe(user_id="ID do usu\u00e1rio Discord")
async def setadmin(i: discord.Interaction, user_id: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    uid = int(user_id)
    if uid not in CONFIG["admin_ids"]:
        CONFIG["admin_ids"].append(uid)
    await i.response.send_message(f"Admin <@{uid}> adicionado.", ephemeral=True)

@admin_group.command(name="setpix", description="Configurar chave PIX")
@app_commands.describe(chave="Chave PIX", valor="Valor padr\u00e3o (ex: 25.00)")
async def setpix(i: discord.Interaction, chave: str, valor: str = "25.00"):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    CONFIG["pix_key"] = chave
    CONFIG["pix_value"] = valor
    await i.response.send_message(f"PIX configurado: `{chave}` (R${valor})", ephemeral=True)

@admin_group.command(name="setcategoria", description="Configurar categoria de tickets")
@app_commands.describe(id="ID da categoria")
async def setcategoria(i: discord.Interaction, id: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    CONFIG["ticket_category_id"] = int(id)
    await i.response.send_message(f"Categoria configurada: {id}", ephemeral=True)

@admin_group.command(name="aprovar", description="Aprovar pagamento PIX e entregar key")
@app_commands.describe(membro="Membro do Discord")
async def aprovar(i: discord.Interaction, membro: discord.Member):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    db = get_db()
    if using_pg:
        rows = db_execall("SELECT * FROM tickets WHERE user_id = %s AND status = 'open' ORDER BY id DESC LIMIT 1", (membro.id,))
    else:
        rows = db_execall("SELECT * FROM tickets WHERE user_id = ? AND status = 'open' ORDER BY id DESC LIMIT 1", (membro.id,))
    ticket = rows[0] if rows else None
    if not ticket:
        return await i.response.send_message("Nenhum ticket aberto encontrado.", ephemeral=True)
    ticket = convert_row(ticket) if not isinstance(ticket, dict) else ticket
    plan_dias = {"diario": 1, "semanal": 7, "mensal": 30, "lifetime": 36500}
    dias = plan_dias.get(ticket.get("plan", ""), 30)
    k = gen_key()
    if using_pg:
        db_exec("INSERT INTO keys (key, duration_days, used_by, used_at, created_at) VALUES (%s, %s, %s, %s, %s)", (k, dias, membro.name, now(), now()))
        db_exec("UPDATE tickets SET status = 'approved', payment_status = 'paid' WHERE id = %s", (ticket["id"],))
    else:
        db_exec("INSERT INTO keys (key, duration_days, used_by, used_at, created_at) VALUES (?, ?, ?, ?, ?)", (k, dias, membro.name, now(), now()))
        db_exec("UPDATE tickets SET status = 'approved', payment_status = 'paid' WHERE id = ?", (ticket["id"],))
        db_commit()
    try:
        await membro.send(f"Sua key foi aprovada! Use o loader para baixar.\nKey: `{k}` ({dias} dias)")
    except:
        pass
    await i.response.send_message(f"Pagamento aprovado! Key `{k}` entregue a {membro.mention}.", ephemeral=True)

@admin_group.command(name="negar", description="Negar pagamento PIX")
@app_commands.describe(membro="Membro do Discord")
async def negar(i: discord.Interaction, membro: discord.Member):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    if using_pg:
        db_exec("UPDATE tickets SET status = 'denied' WHERE user_id = %s AND status = 'open'", (membro.id,))
    else:
        db_exec("UPDATE tickets SET status = 'denied' WHERE user_id = ? AND status = 'open'", (membro.id,))
        db_commit()
    try:
        await membro.send("Seu pagamento foi recusado. Entre em contato com o suporte.")
    except:
        pass
    await i.response.send_message(f"Pagamento de {membro.mention} recusado.", ephemeral=True)

bot.tree.add_command(admin_group)

@bot.tree.command(name="ticket", description="Postar o painel de vendas")
async def cmd_ticket(i: discord.Interaction):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    from views import MainPanel
    BANNER = "https://i.imgur.com/DSxv3VU.png"
    embed = discord.Embed(title="Satella Private", color=0xDB00A6,
        description="**Bem-vindo a central de atendimento!**\n\n\U0001f4b0 **Vendas** — Adquira ou renove sua licenca\n\U0001f4ac **Suporte** — Duvidas ou problemas tecnicos\n\nEscolha uma opcao abaixo:")
    embed.set_image(url=BANNER)
    embed.add_field(name="\U0001f48e Planos Disponiveis", value="\U0001f4a5 **Diario** — R$15,00 *(1 dia)*\n\U0001f550 **Semanal** — R$50,00 *(7 dias)*\n\U0001f4c5 **Mensal** — R$100,00 *(30 dias)*\n\u26a1 **Lifetime** — R$500,00 *(Vitalicio)*", inline=False)
    embed.set_footer(text="Satella Private", icon_url=bot.user.display_avatar.url if bot.user else None)
    await i.channel.send(embed=embed, view=MainPanel())
    await i.response.send_message("Painel postado!", ephemeral=True)

# ========== FLASK / API ==========

app = Flask(__name__)

@app.route("/")
def serve_panel():
    idx_path = os.path.join(script_dir, "index.html")
    with open(idx_path, encoding="utf-8") as f:
        return f.read()

admin_tokens = {}

@app.route("/api/admin/login", methods=["POST", "OPTIONS"])
def api_admin_login():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    data = request.get_json() or {}
    user = data.get("username", "").strip().lower()
    pw = data.get("password", "").strip()
    admins = CONFIG.get("admins", {})
    if user in admins and admins[user] == pw:
        t = make_token()
        admin_tokens[t] = {"username": user, "created": now()}
        return jsonify({"success": True, "token": t, "username": user})
    return jsonify({"success": False, "error": "Credenciais inv\u00e1lidas"})

def require_admin():
    auth = request.headers.get("Authorization", "").replace("Bearer ", "").strip()
    if auth not in admin_tokens:
        return None
    return admin_tokens[auth]["username"]

@app.route("/api/admin/stats", methods=["POST", "OPTIONS"])
def api_admin_stats():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    if not require_admin():
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    total_keys = 0; used_keys = 0; avail_keys = 0
    total_users = 0; active_users = 0; banned_users = 0
    try:
        if using_pg:
            r = db_execone("SELECT COUNT(*) as c FROM keys"); total_keys = r["c"] if r else 0
            r = db_execone("SELECT COUNT(*) as c FROM keys WHERE used_by IS NOT NULL"); used_keys = r["c"] if r else 0
            r = db_execone("SELECT COUNT(*) as c FROM users"); total_users = r["c"] if r else 0
            r = db_execone("SELECT COUNT(*) as c FROM users WHERE banned = 1"); banned_users = r["c"] if r else 0
            r = db_execone("SELECT COUNT(*) as c FROM users WHERE banned = 0 AND expires_at > %s", (now(),)); active_users = r["c"] if r else 0
        else:
            with lock:
                total_keys = get_db().execute("SELECT COUNT(*) as c FROM keys").fetchone()["c"]
                used_keys = get_db().execute("SELECT COUNT(*) as c FROM keys WHERE used_by IS NOT NULL").fetchone()["c"]
                total_users = get_db().execute("SELECT COUNT(*) as c FROM users").fetchone()["c"]
                banned_users = get_db().execute("SELECT COUNT(*) as c FROM users WHERE banned = 1").fetchone()["c"]
                active_users = get_db().execute("SELECT COUNT(*) as c FROM users WHERE banned = 0 AND expires_at > ?", (now(),)).fetchone()["c"]
        avail_keys = total_keys - used_keys
    except:
        pass
    return jsonify({"success": True, "total_keys": total_keys, "used_keys": used_keys, "avail_keys": avail_keys, "total_users": total_users, "active_users": active_users, "banned_users": banned_users})

@app.route("/api/admin/keys", methods=["POST", "OPTIONS"])
def api_admin_keys():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    if not require_admin():
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    if using_pg:
        rows = db_execall("SELECT id, key, duration_days, used_by, used_at, created_at FROM keys ORDER BY id DESC LIMIT 200")
    else:
        with lock:
            rows = get_db().execute("SELECT id, key, duration_days, used_by, used_at, created_at FROM keys ORDER BY id DESC LIMIT 200").fetchall()
    keys_list = []
    for r in rows:
        r = convert_row(r)
        keys_list.append({"id": r["id"], "key": r["key"], "duration_days": r["duration_days"], "used_by": r["used_by"], "used_at": r["used_at"], "created_at": r["created_at"]})
    return jsonify({"success": True, "keys": keys_list})

@app.route("/api/admin/keys/generate", methods=["POST", "OPTIONS"])
def api_admin_keys_generate():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    qty = int(data.get("quantity", 1))
    days = int(data.get("days", 30))
    if qty < 1 or qty > 50:
        return jsonify({"success": False, "error": "Quantidade entre 1 e 50"})
    if days < 0 or days > 3650:
        return jsonify({"success": False, "error": "Dias entre 0 e 3650"})
    created = []
    for _ in range(qty):
        k = gen_key()
        if using_pg:
            db_exec("INSERT INTO keys (key, duration_days, created_at) VALUES (%s, %s, %s)", (k, days, now()))
        else:
            get_db().execute("INSERT INTO keys (key, duration_days, created_at) VALUES (?, ?, ?)", (k, days, now()))
        created.append(k)
    if not using_pg:
        get_db().commit()
    log_action("admin_genkey", admin, f"{qty}x {days}d", request.remote_addr)
    return jsonify({"success": True, "keys": created, "quantity": qty})

@app.route("/api/admin/keys/add", methods=["POST", "OPTIONS"])
def api_admin_keys_add():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    key_val = data.get("key", "").strip().upper()
    days = int(data.get("days", 30))
    if not key_val or len(key_val) < 5:
        return jsonify({"success": False, "error": "Key inv\u00e1lida"})
    try:
        if using_pg:
            db_exec("INSERT INTO keys (key, duration_days, created_at) VALUES (%s, %s, %s)", (key_val, days, now()))
        else:
            get_db().execute("INSERT INTO keys (key, duration_days, created_at) VALUES (?, ?, ?)", (key_val, days, now()))
            get_db().commit()
        log_action("admin_addkey", admin, key_val[:16], request.remote_addr)
        return jsonify({"success": True})
    except Exception as e:
        if "UNIQUE" in str(e) or "duplicate" in str(e):
            return jsonify({"success": False, "error": "Key j\u00e1 existe"})
        return jsonify({"success": False, "error": str(e)})

@app.route("/api/admin/keys/delete", methods=["POST", "OPTIONS"])
def api_admin_keys_delete():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    key_id = data.get("id")
    if using_pg:
        db_exec("DELETE FROM keys WHERE id = %s", (key_id,))
    else:
        get_db().execute("DELETE FROM keys WHERE id = ?", (key_id,))
        get_db().commit()
    log_action("admin_delkey", admin, f"id:{key_id}", request.remote_addr)
    return jsonify({"success": True})

@app.route("/api/admin/keys/delete_by_value", methods=["POST", "OPTIONS"])
def api_admin_keys_delete_by_value():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    key_val = data.get("key", "").strip().upper()
    if not key_val:
        return jsonify({"success": False, "error": "Key obrigat\u00f3ria"})
    if using_pg:
        db_exec("DELETE FROM keys WHERE key = %s AND used_by IS NULL", (key_val,))
    else:
        get_db().execute("DELETE FROM keys WHERE key = ? AND used_by IS NULL", (key_val,))
        get_db().commit()
    log_action("admin_delkey_val", admin, key_val[:16], request.remote_addr)
    return jsonify({"success": True})

@app.route("/api/admin/keys/addtime", methods=["POST", "OPTIONS"])
def api_admin_keys_addtime():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    key_val = data.get("key", "").strip().upper()
    extra_days = int(data.get("days", 30))
    if not key_val or extra_days < 1:
        return jsonify({"success": False, "error": "Key e dias obrigat\u00f3rios"})
    r = db_execone("SELECT * FROM keys WHERE key = %s" if using_pg else "SELECT * FROM keys WHERE key = ?", (key_val,))
    if not r:
        try:
            if using_pg:
                db_exec("INSERT INTO keys (key, duration_days, created_at) VALUES (%s, %s, %s)", (key_val, extra_days, now()))
            else:
                get_db().execute("INSERT INTO keys (key, duration_days, created_at) VALUES (?, ?, ?)", (key_val, extra_days, now()))
                get_db().commit()
            log_action("admin_addtime_newkey", admin, f"{key_val[:16]} {extra_days}d", request.remote_addr)
            return jsonify({"success": True, "message": f"Key criada com {extra_days} dias"})
        except:
            return jsonify({"success": False, "error": "Erro ao criar key"})
    r = convert_row(r)
    if not r["used_by"]:
        if using_pg:
            db_exec("UPDATE keys SET duration_days = duration_days + %s WHERE key = %s", (extra_days, key_val))
        else:
            get_db().execute("UPDATE keys SET duration_days = duration_days + ? WHERE key = ?", (extra_days, key_val))
            get_db().commit()
        log_action("admin_addtime", admin, f"{key_val[:16]} +{extra_days}d", request.remote_addr)
        return jsonify({"success": True, "message": f"Adicionado {extra_days} dias \u00e0 key"})
    username = r["used_by"]
    user = db_execone("SELECT * FROM users WHERE username = %s" if using_pg else "SELECT * FROM users WHERE username = ?", (username,))
    if user:
        user = convert_row(user)
        extra_secs = extra_days * 86400
        new_expires = max(user["expires_at"], now()) + extra_secs
        if using_pg:
            db_exec("UPDATE users SET expires_at = %s WHERE username = %s", (new_expires, username))
        else:
            get_db().execute("UPDATE users SET expires_at = ? WHERE username = ?", (new_expires, username))
            get_db().commit()
        log_action("admin_addtime_used", admin, f"{key_val[:16]}->{username} +{extra_days}d", request.remote_addr)
        return jsonify({"success": True, "message": f"Adicionado {extra_days} dias ao usu\u00e1rio {username}"})
    return jsonify({"success": False, "error": "Usu\u00e1rio n\u00e3o encontrado"})

@app.route("/api/admin/users", methods=["POST", "OPTIONS"])
def api_admin_users():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    if not require_admin():
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    if using_pg:
        rows = db_execall("SELECT id, username, hwid, created_at, expires_at, banned FROM users ORDER BY id DESC LIMIT 200")
    else:
        with lock:
            rows = get_db().execute("SELECT id, username, hwid, created_at, expires_at, banned FROM users ORDER BY id DESC LIMIT 200").fetchall()
    users_list = []
    for r in rows:
        r = convert_row(r)
        users_list.append({"id": r["id"], "username": r["username"], "hwid": r["hwid"], "created_at": r["created_at"], "expires_at": r["expires_at"], "banned": bool(r["banned"]), "expired": r["expires_at"] < now() if r["expires_at"] else True})
    return jsonify({"success": True, "users": users_list})

@app.route("/api/admin/users/ban", methods=["POST", "OPTIONS"])
def api_admin_users_ban():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    if using_pg:
        db_exec("UPDATE users SET banned = 1 WHERE username = %s", (username,))
    else:
        get_db().execute("UPDATE users SET banned = 1 WHERE username = ?", (username,))
        get_db().commit()
    log_action("admin_ban", admin, username, request.remote_addr)
    return jsonify({"success": True})

@app.route("/api/admin/users/unban", methods=["POST", "OPTIONS"])
def api_admin_users_unban():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    if using_pg:
        db_exec("UPDATE users SET banned = 0 WHERE username = %s", (username,))
    else:
        get_db().execute("UPDATE users SET banned = 0 WHERE username = ?", (username,))
        get_db().commit()
    log_action("admin_unban", admin, username, request.remote_addr)
    return jsonify({"success": True})

@app.route("/api/admin/users/reset_hwid", methods=["POST", "OPTIONS"])
def api_admin_users_reset_hwid():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    if using_pg:
        db_exec("UPDATE users SET hwid = '' WHERE username = %s", (username,))
    else:
        get_db().execute("UPDATE users SET hwid = '' WHERE username = ?", (username,))
        get_db().commit()
    log_action("admin_reset_hwid", admin, username, request.remote_addr)
    return jsonify({"success": True})

@app.route("/api/admin/logs", methods=["POST", "OPTIONS"])
def api_admin_logs():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    if not require_admin():
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    if using_pg:
        rows = db_execall("SELECT * FROM logs ORDER BY id DESC LIMIT 100")
    else:
        with lock:
            rows = get_db().execute("SELECT * FROM logs ORDER BY id DESC LIMIT 100").fetchall()
    return jsonify({"success": True, "logs": [dict(r) if not isinstance(r, dict) else r for r in rows]})

@app.route("/api/admin/config", methods=["POST", "OPTIONS"])
def api_admin_config():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    if data.get("action") == "get":
        return jsonify({"success": True, "config": {"pix_key": CONFIG.get("pix_key", ""), "pix_value": CONFIG.get("pix_value", "25.00")}})
    if data.get("action") == "set":
        if "pix_key" in data: CONFIG["pix_key"] = data["pix_key"]
        if "pix_value" in data: CONFIG["pix_value"] = data["pix_value"]
        if "webhook_url" in data: CONFIG["webhook_url"] = data["webhook_url"]
        log_action("admin_config", admin, "config atualizada", request.remote_addr)
        return jsonify({"success": True, "message": "Configura\u00e7\u00e3o salva"})
    return jsonify({"success": False, "error": "A\u00e7\u00e3o inv\u00e1lida"})

@app.route("/api/login", methods=["POST", "OPTIONS"])
def api_login():
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    password = data.get("password", "").strip()
    hwid = data.get("hwid", "").strip()
    if not username or not password:
        return jsonify({"success": False, "error": "Username e senha obrigat\u00f3rios"})
    r = db_execone("SELECT * FROM users WHERE username = %s" if using_pg else "SELECT * FROM users WHERE username = ?", (username,))
    if not r:
        return jsonify({"success": False, "error": "Usu\u00e1rio n\u00e3o encontrado"})
    r = convert_row(r)
    if r["banned"]:
        log_action("login_banned", username, f"IP:{request.remote_addr}", request.remote_addr)
        return jsonify({"success": False, "error": "Usu\u00e1rio banido"})
    if r["password_hash"] != hash_pw(password):
        log_action("login_fail", username, "senha incorreta", request.remote_addr)
        return jsonify({"success": False, "error": "Senha incorreta"})
    if r["expires_at"] < now():
        log_action("login_expired", username, "subscription expirada", request.remote_addr)
        return jsonify({"success": False, "error": "Subscription expirada"})
    if r["hwid"] and r["hwid"] != hwid:
        log_action("login_hwid_mismatch", username, "hwid diferente", request.remote_addr)
        return jsonify({"success": False, "error": "HWID n\u00e3o corresponde. Solicite reset ao admin."})
    if not r["hwid"] and hwid:
        if using_pg:
            db_exec("UPDATE users SET hwid = %s WHERE username = %s", (hwid, username))
        else:
            get_db().execute("UPDATE users SET hwid = ? WHERE username = ?", (hwid, username))
            get_db().commit()
    token = make_token()
    tokens[token] = {"username": username, "expires_at": r["expires_at"], "created": now()}
    log_action("login_ok", username, f"token:{token[:8]}...", request.remote_addr)
    return jsonify({"success": True, "token": token, "username": username, "expires_at": r["expires_at"], "expires_in_days": max(0, (r["expires_at"] - now()) // 86400)})

@app.route("/api/register", methods=["POST", "OPTIONS"])
def api_register():
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    password = data.get("password", "").strip()
    key = data.get("key", "").strip()
    hwid = data.get("hwid", "").strip()
    if not username or not password or not key:
        return jsonify({"success": False, "error": "Username, senha e key obrigat\u00f3rios"})
    if len(username) < 3 or len(username) > 20:
        return jsonify({"success": False, "error": "Username deve ter 3-20 caracteres"})
    if len(password) < 4:
        return jsonify({"success": False, "error": "Senha deve ter no m\u00ednimo 4 caracteres"})
    key_up = key.upper().strip()
    key_row = db_execone("SELECT * FROM keys WHERE key = %s AND used_by IS NULL" if using_pg else "SELECT * FROM keys WHERE key = ? AND used_by IS NULL", (key_up,))
    if not key_row:
        log_action("register_badkey", username, f"key inv\u00e1lida: {key[:8]}...", request.remote_addr)
        return jsonify({"success": False, "error": "Chave inv\u00e1lida ou j\u00e1 usada"})
    key_row = convert_row(key_row)
    try:
        if using_pg:
            db_exec("INSERT INTO users (username, password_hash, hwid, created_at, expires_at) VALUES (%s, %s, %s, %s, %s)", (username, hash_pw(password), hwid, now(), now() + key_row["duration_days"] * 86400))
            db_exec("UPDATE keys SET used_by = %s, used_at = %s WHERE key = %s", (username, now(), key_row["key"]))
        else:
            get_db().execute("INSERT INTO users (username, password_hash, hwid, created_at, expires_at) VALUES (?, ?, ?, ?, ?)", (username, hash_pw(password), hwid, now(), now() + key_row["duration_days"] * 86400))
            get_db().execute("UPDATE keys SET used_by = ?, used_at = ? WHERE key = ?", (username, now(), key_row["key"]))
            get_db().commit()
    except Exception as e:
        if "UNIQUE" in str(e) or "duplicate" in str(e):
            return jsonify({"success": False, "error": "Username j\u00e1 existe"})
        return jsonify({"success": False, "error": str(e)})
    token = make_token()
    tokens[token] = {"username": username, "expires_at": now() + key_row["duration_days"] * 86400, "created": now()}
    log_action("register_ok", username, f"key:{key[:8]}... {key_row['duration_days']}d", request.remote_addr)
    return jsonify({"success": True, "token": token, "username": username, "expires_in_days": key_row["duration_days"]})

@app.route("/api/verify", methods=["POST", "OPTIONS"])
def api_verify():
    data = request.get_json() or {}
    token = data.get("token", "").strip()
    if not token or token not in tokens:
        return jsonify({"success": False, "error": "Token inv\u00e1lido"})
    sess = tokens[token]
    r = db_execone("SELECT * FROM users WHERE username = %s" if using_pg else "SELECT * FROM users WHERE username = ?", (sess["username"],))
    if not r:
        tokens.pop(token, None)
        return jsonify({"success": False, "error": "Usu\u00e1rio n\u00e3o encontrado ou banido"})
    r = convert_row(r)
    if r["banned"]:
        tokens.pop(token, None)
        return jsonify({"success": False, "error": "Usu\u00e1rio banido"})
    if r["expires_at"] < now():
        tokens.pop(token, None)
        return jsonify({"success": False, "error": "Subscription expirada"})
    sess["expires_at"] = r["expires_at"]
    return jsonify({"success": True, "username": r["username"], "expires_at": r["expires_at"], "expires_in_days": max(0, (r["expires_at"] - now()) // 86400), "hwid": r["hwid"]})

@app.route("/api/check", methods=["POST", "OPTIONS"])
def api_check():
    data = request.get_json() or {}
    username = data.get("username", "").strip()
    if not username:
        return jsonify({"success": False})
    r = db_execone("SELECT username, expires_at, banned FROM users WHERE username = %s" if using_pg else "SELECT username, expires_at, banned FROM users WHERE username = ?", (username,))
    if not r:
        return jsonify({"success": False, "error": "not_found"})
    r = convert_row(r)
    return jsonify({"success": True, "exists": True, "banned": bool(r["banned"]), "expired": r["expires_at"] < now()})

def log_action(action, username, detail="", ip=""):
    try:
        if using_pg:
            db_exec("INSERT INTO logs (action, username, detail, ip, timestamp) VALUES (%s, %s, %s, %s, %s)", (action, username, detail, ip, now()))
        else:
            get_db().execute("INSERT INTO logs (action, username, detail, ip, timestamp) VALUES (?, ?, ?, ?, ?)", (action, username, detail, ip, now()))
            get_db().commit()
    except:
        pass

@app.route("/api/logs", methods=["GET"])
def api_logs():
    auth = request.headers.get("Authorization", "")
    if auth not in [f"Bearer {t}" for t in tokens if tokens[t].get("admin")]:
        return jsonify({"success": False, "error": "Unauthorized"})
    if using_pg:
        rows = db_execall("SELECT * FROM logs ORDER BY id DESC LIMIT 100")
    else:
        with lock:
            rows = get_db().execute("SELECT * FROM logs ORDER BY id DESC LIMIT 100").fetchall()
    return jsonify({"success": True, "logs": [dict(r) if not isinstance(r, dict) else r for r in rows]})

def send_webhook(content: str):
    wh_url = CONFIG.get("webhook_url", "")
    if not wh_url or "AQUI" in wh_url:
        return
    try:
        wh_data = json.dumps({"content": content})
        req = urllib.request.Request(wh_url, data=wh_data.encode(), headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=5)
    except:
        pass

@app.route("/api/notify_download", methods=["POST", "OPTIONS"])
def api_notify_download():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    data = request.get_json() or {}
    username = data.get("username", "")
    ip = request.remote_addr or ""
    send_webhook(f"\U0001f4e5 **Download Satella.dll**\nUsu\u00e1rio: `{username}`\nIP: `{ip}`")
    log_action("download", username, f"IP:{ip}", ip)
    return jsonify({"success": True})

@app.route("/api/download", methods=["GET", "OPTIONS"])
def api_download():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    token = request.args.get("token", "").strip()
    if not token or token not in tokens:
        return jsonify({"success": False, "error": "Token inv\u00e1lido"}), 401
    dll_path = os.path.join(script_dir, "..", "server", "Satella.dll")
    if not os.path.exists(dll_path):
        dll_path = os.path.join(script_dir, "Satella.dll")
        if not os.path.exists(dll_path):
            return jsonify({"success": False, "error": "DLL n\u00e3o encontrada no servidor"}), 404
    username = tokens[token]["username"]
    ip = request.remote_addr or ""
    send_webhook(f"\U0001f4e5 **Download Satella.dll**\nUsu\u00e1rio: `{username}`\nIP: `{ip}`")
    log_action("download", username, f"IP:{ip}", ip)
    return app.response_class(
        response=open(dll_path, "rb"),
        mimetype="application/octet-stream",
        headers={"Content-Disposition": "attachment; filename=Satella.dll"}
    )

@app.route("/api/admin/announce", methods=["POST", "OPTIONS"])
def api_admin_announce():
    if request.method == "OPTIONS":
        return jsonify({"success": True})
    admin = require_admin()
    if not admin:
        return jsonify({"success": False, "error": "N\u00e3o autorizado"})
    data = request.get_json() or {}
    msg = data.get("message", "").strip()
    if not msg:
        return jsonify({"success": False, "error": "Mensagem obrigat\u00f3ria"})
    send_webhook(f"\U0001f4e2 **An\u00fancio** ({admin}):\n{msg}")
    log_action("announce", admin, msg[:50], request.remote_addr)
    return jsonify({"success": True})

# ========== SALES (merged from messaging_bot) ==========

ROSA = 0xDB00A6
BANNER = "https://i.imgur.com/DSxv3VU.png"

PLANOS = {
    "diario":   {"nome": "Diario",   "preco": 15,  "dias": 1,     "emoji": "\U0001f4a5"},
    "semanal":  {"nome": "Semanal",  "preco": 50,  "dias": 7,     "emoji": "\U0001f550"},
    "mensal":   {"nome": "Mensal",   "preco": 100, "dias": 30,    "emoji": "\U0001f4c5"},
    "lifetime": {"nome": "Lifetime", "preco": 500, "dias": 36500, "emoji": "\u26a1"},
}

async def enviar_key_dm(member: discord.Member, key: str, dias: int, plano_nome: str):
    embed = discord.Embed(
        title="\u2705 Pagamento Aprovado!",
        color=discord.Color.green(),
        description=(
            f"**Sua key foi gerada:**\n"
            f"```\n{key}\n```\n"
            f"**Plano:** {plano_nome}\n"
            f"**Duração:** {dias} dia(s)\n\n"
            f"Use o loader para baixar o Satella.\n"
            f"Obrigado pela preferência! \u2764"
        )
    )
    embed.set_image(url=BANNER)
    embed.set_footer(text="Satella Private")
    try:
        await member.send(embed=embed)
        return True
    except:
        return False

async def aprovar_pagamento(member: discord.Member, plan: str, txid: str = None):
    info = PLANOS.get(plan)
    if not info:
        return None
    dias = info["dias"]
    k = gen_key()
    try:
        if using_pg:
            db_exec("INSERT INTO keys (key, duration_days, used_by, used_at, created_at) VALUES (%s, %s, %s, %s, %s)",
                    (k, dias, member.name, now(), now()))
        else:
            db_exec("INSERT INTO keys (key, duration_days, used_by, used_at, created_at) VALUES (?, ?, ?, ?, ?)",
                    (k, dias, member.name, now(), now()))
            db_commit()
    except Exception as e:
        print(f"[SALES] Erro ao inserir key: {e}")
        return None
    if txid:
        try:
            if using_pg:
                db_exec("UPDATE pix_pending SET status = 'paid' WHERE txid = %s", (txid,))
            else:
                db_exec("UPDATE pix_pending SET status = 'paid' WHERE txid = ?", (txid,))
                db_commit()
        except:
            pass
    role_id = CONFIG.get("client_role_id", 0)
    if role_id and isinstance(member, discord.Member):
        role = member.guild.get_role(role_id)
        if role:
            try:
                await member.add_roles(role)
            except:
                pass
    await enviar_key_dm(member, k, dias, info["nome"])
    await log_action(f"APROVADO: {member} ({member.id}) plano={plan} key={k}")
    return k

class PainelView(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Adquirir", style=discord.ButtonStyle.success, emoji="\U0001f6d2")
    async def adquirir(self, i: discord.Interaction, b: discord.ui.Button):
        await i.response.send_message("Selecione o plano desejado:", view=PlanSelect(), ephemeral=True)

class PlanSelect(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.select(placeholder="Escolha seu plano...", options=[
        discord.SelectOption(label="Diario - R$15", description="1 dia de acesso", value="diario", emoji="\U0001f4a5"),
        discord.SelectOption(label="Semanal - R$50", description="7 dias de acesso", value="semanal", emoji="\U0001f550"),
        discord.SelectOption(label="Mensal - R$100", description="30 dias de acesso", value="mensal", emoji="\U0001f4c5"),
        discord.SelectOption(label="Lifetime - R$500", description="Acesso vitalicio", value="lifetime", emoji="\u26a1"),
    ])
    async def select_plan(self, i: discord.Interaction, s: discord.ui.Select):
        info = PLANOS.get(s.values[0])
        if not info:
            return await i.response.send_message("Plano invalido.", ephemeral=True)
        txid = secrets.token_hex(8)
        valor = info["preco"]
        chave = CONFIG.get("pix_key", "")
        if not chave:
            return await i.response.send_message("PIX nao configurado.", ephemeral=True)
        nome = CONFIG.get("pix_name", "Satella")
        brcode = gerar_pix_brcode(chave, valor, nome, txid=txid)
        qr_url = f"https://chart.googleapis.com/chart?chs=250x250&cht=qr&chl={brcode}"
        try:
            if using_pg:
                db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (%s, %s, %s, 'pending', %s)",
                        (txid, str(i.user.id), s.values[0], now()))
            else:
                db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (?, ?, ?, 'pending', ?)",
                        (txid, str(i.user.id), s.values[0], now()))
                db_commit()
        except:
            try:
                if not using_pg:
                    db_exec("CREATE TABLE IF NOT EXISTS pix_pending (id INTEGER PRIMARY KEY AUTOINCREMENT, txid TEXT UNIQUE, user_id TEXT, plan TEXT, status TEXT DEFAULT 'pending', created_at INTEGER)")
                    db_commit()
                    db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (?, ?, ?, 'pending', ?)",
                            (txid, str(i.user.id), s.values[0], now()))
                    db_commit()
            except Exception as e:
                return await i.response.send_message(f"Erro: {e}", ephemeral=True)
        embed = discord.Embed(
            title=f"\U0001f4b0 Carrinho - {info['nome']}",
            color=ROSA,
            description=(
                f"**Plano:** {info['emoji']} {info['nome']}\n"
                f"**Valor:** R$ **{valor:.2f}**\n\n"
                f"\U0001f449 **Pague via PIX**\n\n"
                f"**Chave:** `{chave}`\n"
                f"**Nome:** {nome}\n\n"
                f"Pague o valor exato de **R$ {valor:.2f}**\n"
                f"Depois clique em **\"Já paguei!\"** abaixo."
            )
        )
        embed.set_image(url=qr_url)
        embed.set_footer(text=f"Satella Private • ID: {txid[:8]}...")
        view = ComprovanteView(txid, s.values[0])
        try:
            await i.user.send(embed=embed, view=view)
            await i.response.send_message("\U0001f4e8 **Carrinho enviado no seu privado!** Verifique suas DMs.", ephemeral=True)
        except discord.Forbidden:
            await i.response.send_message(
                f"\u274c Nao consegui enviar DM. Ative \"Permitir mensagens diretas\" no servidor.\n\n"
                f"**Chave PIX:** `{chave}`\n"
                f"**Valor:** R$ {valor:.2f}\n"
                f"**QR Code:** {qr_url}",
                ephemeral=True)

class ComprovanteModal(discord.ui.Modal, title="Enviar Comprovante"):
    def __init__(self, txid: str, plan: str):
        super().__init__()
        self.txid = txid
        self.plan = plan
        self.add_item(discord.ui.TextInput(
            label="Link do comprovante",
            placeholder="Cole o link da imagem (imgur, discord, etc)",
            style=discord.TextStyle.short,
            required=True,
        ))

    async def on_submit(self, i: discord.Interaction):
        link = self.children[0].value.strip()
        embed_comprovante = discord.Embed(
            title="\U0001f4e9 Comprovante Recebido",
            color=discord.Color.green(),
            description="Seu comprovante foi enviado para aprovação. Aguarde.",
        )
        embed_comprovante.set_image(url=link)
        embed_comprovante.set_footer(text="Aguardando aprovacao do admin")
        await i.response.send_message(embed=embed_comprovante)
        admin_log = discord.Embed(
            title="\U0001f4e9 Novo Comprovante",
            color=ROSA,
            description=(
                f"**Comprador:** {i.user.mention} (`{i.user.id}`)\n"
                f"**Plano:** {self.plan}\n"
                f"**TxID:** `{self.txid}`\n"
                f"**Comprovante:** [Clique aqui]({link})"
            )
        )
        admin_log.set_image(url=link)
        log_id = CONFIG.get("log_channel_id")
        if log_id:
            ch = bot.get_channel(log_id)
            if ch:
                await ch.send(
                    embed=admin_log,
                    content=" ".join(f"<@{uid}>" for uid in CONFIG.get("admin_ids", []) if uid),
                    view=AdminApproveView(self.txid, self.plan, i.user.id)
                )

class ComprovanteView(discord.ui.View):
    def __init__(self, txid: str = "", plan: str = ""):
        super().__init__(timeout=None)
        self.txid = txid
        self.plan = plan

    @discord.ui.button(label="Ja paguei!", style=discord.ButtonStyle.success, emoji="\u2705")
    async def ja_paguei(self, i: discord.Interaction, b: discord.ui.Button):
        modal = ComprovanteModal(self.txid, self.plan)
        await i.response.send_modal(modal)

class AdminApproveView(discord.ui.View):
    def __init__(self, txid: str = "", plan: str = "", user_id: int = 0):
        super().__init__(timeout=None)
        self.txid = txid
        self.plan = plan
        self.user_id = user_id

    @discord.ui.button(label="Aprovar", style=discord.ButtonStyle.success, emoji="\u2705")
    async def approve_btn(self, i: discord.Interaction, b: discord.ui.Button):
        if i.user.id not in CONFIG.get("admin_ids", []):
            return await i.response.send_message("Sem permissao.", ephemeral=True)
        member = i.guild.get_member(self.user_id)
        if not member:
            return await i.response.send_message("Usuario nao encontrado no servidor.", ephemeral=True)
        await i.response.defer(ephemeral=True)
        k = await aprovar_pagamento(member, self.plan, self.txid)
        if k:
            embed = discord.Embed(title="\u2705 Pagamento Aprovado", color=discord.Color.green(),
                description=f"**Admin:** {i.user.mention}\n**Comprador:** <@{self.user_id}>\n**Key:** `{k}`\n\n\U0001f4e5 Key enviada no PV.")
            await i.channel.send(embed=embed)
            await i.followup.send("Pagamento aprovado com sucesso!", ephemeral=True)
            for child in self.children:
                child.disabled = True
            await i.message.edit(view=self)
        else:
            await i.followup.send("Erro ao aprovar pagamento.", ephemeral=True)

    @discord.ui.button(label="Negar", style=discord.ButtonStyle.danger, emoji="\u274c")
    async def deny_btn(self, i: discord.Interaction, b: discord.ui.Button):
        if i.user.id not in CONFIG.get("admin_ids", []):
            return await i.response.send_message("Sem permissao.", ephemeral=True)
        try:
            if using_pg:
                db_exec("UPDATE pix_pending SET status = 'denied' WHERE txid = %s", (self.txid,))
            else:
                db_exec("UPDATE pix_pending SET status = 'denied' WHERE txid = ?", (self.txid,))
                db_commit()
        except:
            pass
        member = i.guild.get_member(self.user_id)
        if member:
            try:
                await member.send("\u274c Seu pagamento foi recusado. Contacte o suporte.")
            except:
                pass
        embed = discord.Embed(title="\u274c Pagamento Recusado", color=discord.Color.red(),
            description=f"**Admin:** {i.user.mention}\n**Comprador:** <@{self.user_id}>")
        await i.channel.send(embed=embed)
        await i.response.send_message("Pagamento recusado.", ephemeral=True)
        for child in self.children:
            child.disabled = True
        await i.message.edit(view=self)

@bot.tree.command(name="painel", description="Enviar painel de vendas")
async def cmd_painel(i: discord.Interaction):
    if not is_admin(i):
        return await i.response.send_message("Sem permissao.", ephemeral=True)
    embed = discord.Embed(
        title="\U0001f48e Satella Private",
        color=ROSA,
        description=(
            "\u26a1 **O MELHOR BYPASS DO MERCADO!** \u26a1\n\n"
            "\U0001f6e1 **Anti-Detect:** Não cai em apostados \u274c\n"
            "\U0001f3af **Legit Perfeito:** Suave e preciso \U0001f4af\n"
            "\U0001f680 **Otimizado:** Roda liso em qualquer PC \U0001f4bb\n"
            "\U0001f512 **Seguro:** Atualização constante \U0001f504\n"
            "\U0001f525 **Bypass potente:** Passe batido pelos anti-cheats \ud83d\udcaa\n\n"
            "\U0001f447 **Garanta já o seu acesso e domine o jogo!** \U0001f447"
        )
    )
    embed.set_image(url=BANNER)
    embed.add_field(name="\U0001f4b0 Planos Disponiveis", value=(
        "\U0001f4a5 **Diario** — R$15,00 *(1 dia)*\n"
        "\U0001f550 **Semanal** — R$50,00 *(7 dias)*\n"
        "\U0001f4c5 **Mensal** — R$100,00 *(30 dias)*\n"
        "\u26a1 **Lifetime** — R$500,00 *(Vitalicio)*"
    ), inline=False)
    embed.set_footer(text="Satella Private", icon_url=bot.user.display_avatar.url if bot.user else None)
    await i.channel.send(embed=embed, view=PainelView())
    await i.response.send_message("Painel postado!", ephemeral=True)

@bot.tree.command(name="comprar", description="Iniciar processo de compra")
@app_commands.describe(plano="Plano desejado")
@app_commands.choices(plano=[
    app_commands.Choice(name="Diario - R$15 (1 dia)", value="diario"),
    app_commands.Choice(name="Semanal - R$50 (7 dias)", value="semanal"),
    app_commands.Choice(name="Mensal - R$100 (30 dias)", value="mensal"),
    app_commands.Choice(name="Lifetime - R$500 (Vitalicio)", value="lifetime"),
])
async def cmd_comprar(i: discord.Interaction, plano: str):
    info = PLANOS.get(plano)
    if not info:
        return await i.response.send_message("Plano invalido.", ephemeral=True)
    txid = secrets.token_hex(8)
    valor = info["preco"]
    chave = CONFIG.get("pix_key", "")
    if not chave:
        return await i.response.send_message("PIX nao configurado. Contacte o admin.", ephemeral=True)
    nome = CONFIG.get("pix_name", "Satella")
    brcode = gerar_pix_brcode(chave, valor, nome, txid=txid)
    qr_url = f"https://chart.googleapis.com/chart?chs=250x250&cht=qr&chl={brcode}"
    try:
        if using_pg:
            db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (%s, %s, %s, 'pending', %s)",
                    (txid, str(i.user.id), plano, now()))
        else:
            db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (?, ?, ?, 'pending', ?)",
                    (txid, str(i.user.id), plano, now()))
            db_commit()
    except:
        try:
            if not using_pg:
                db_exec("CREATE TABLE IF NOT EXISTS pix_pending (id INTEGER PRIMARY KEY AUTOINCREMENT, txid TEXT UNIQUE, user_id TEXT, plan TEXT, status TEXT DEFAULT 'pending', created_at INTEGER)")
                db_commit()
                db_exec("INSERT INTO pix_pending (txid, user_id, plan, status, created_at) VALUES (?, ?, ?, 'pending', ?)",
                        (txid, str(i.user.id), plano, now()))
                db_commit()
        except Exception as e:
            return await i.response.send_message(f"Erro ao criar pagamento: {e}", ephemeral=True)
    embed = discord.Embed(
        title=f"\U0001f4b0 Carrinho - {info['nome']}",
        color=ROSA,
        description=(
            f"**Plano:** {info['emoji']} {info['nome']}\n"
            f"**Valor:** R$ **{valor:.2f}**\n\n"
            f"\U0001f449 **Pague via PIX**\n\n"
            f"**Chave:** `{chave}`\n"
            f"**Nome:** {nome}\n\n"
            f"Pague o valor exato de **R$ {valor:.2f}**\n"
            f"Depois clique em **\"Já paguei!\"** abaixo."
        )
    )
    embed.set_image(url=qr_url)
    embed.set_footer(text=f"Satella Private • ID: {txid[:8]}...")
    view = ComprovanteView(txid, plano)
    try:
        await i.user.send(embed=embed, view=view)
        await i.response.send_message("\U0001f4e8 **Carrinho enviado no seu privado!** Verifique suas DMs.", ephemeral=True)
    except discord.Forbidden:
        await i.response.send_message(
            f"\u274c Nao consegui enviar DM. Ative \"Permitir mensagens diretas\" no servidor.\n\n"
            f"**Chave PIX:** `{chave}`\n"
            f"**Valor:** R$ {valor:.2f}\n"
            f"**QR Code:** {qr_url}",
            ephemeral=True)

@bot.tree.command(name="aprovar", description="Aprovar pagamento e entregar key")
@app_commands.describe(usuario="Usuario do Discord")
async def cmd_aprovar(i: discord.Interaction, usuario: discord.Member):
    if not is_admin(i):
        return await i.response.send_message("Sem permissao.", ephemeral=True)
    row = db_execone(
        "SELECT * FROM pix_pending WHERE user_id = ? AND status = 'pending' ORDER BY id DESC LIMIT 1" if not using_pg else "SELECT * FROM pix_pending WHERE user_id = %s AND status = 'pending' ORDER BY id DESC LIMIT 1",
        (str(usuario.id),)
    )
    if not row:
        return await i.response.send_message("Nenhum pagamento pendente para este usuario.", ephemeral=True)
    row = dict(row) if not isinstance(row, dict) else row
    k = await aprovar_pagamento(usuario, row["plan"], row["txid"])
    if k:
        await i.response.send_message(f"\u2705 Pagamento aprovado! Key `{k}` enviada no PV de {usuario.mention}.", ephemeral=True)
    else:
        await i.response.send_message("\u274c Erro ao aprovar.", ephemeral=True)

@bot.tree.command(name="negar", description="Negar pagamento")
@app_commands.describe(usuario="Usuario do Discord")
async def cmd_negar(i: discord.Interaction, usuario: discord.Member):
    if not is_admin(i):
        return await i.response.send_message("Sem permissao.", ephemeral=True)
    row = db_execone(
        "SELECT * FROM pix_pending WHERE user_id = ? AND status = 'pending' ORDER BY id DESC LIMIT 1" if not using_pg else "SELECT * FROM pix_pending WHERE user_id = %s AND status = 'pending' ORDER BY id DESC LIMIT 1",
        (str(usuario.id),)
    )
    if not row:
        return await i.response.send_message("Nenhum pagamento pendente para este usuario.", ephemeral=True)
    row = dict(row) if not isinstance(row, dict) else row
    try:
        if using_pg:
            db_exec("UPDATE pix_pending SET status = 'denied' WHERE txid = %s", (row["txid"],))
        else:
            db_exec("UPDATE pix_pending SET status = 'denied' WHERE txid = ?", (row["txid"],))
            db_commit()
    except:
        pass
    try:
        await usuario.send("\u274c Seu pagamento foi recusado. Contacte o suporte.")
    except:
        pass
    await i.response.send_message(f"\u274c Pagamento de {usuario.mention} recusado.", ephemeral=True)

def cleanup_tokens():
    while True:
        time.sleep(300)
        now_t = now()
        to_del = [t for t, s in tokens.items() if s["expires_at"] < now_t]
        for t in to_del:
            del tokens[t]

def run_flask():
    port = int(os.environ.get("PORT", CONFIG["api_port"]))
    app.run(host="0.0.0.0", port=port, debug=False, use_reloader=False)

def run_bot():
    bot.run(CONFIG["token"])

if __name__ == "__main__":
    print("[AUTH] Iniciando sistema...")
    init_db()
    t1 = threading.Thread(target=run_flask, daemon=True)
    t1.start()
    print(f"[API] Rodando na porta {CONFIG['api_port']}")
    t2 = threading.Thread(target=cleanup_tokens, daemon=True)
    t2.start()
    while True:
        try:
            print("[BOT] Conectando ao Discord...")
            run_bot()
        except Exception as e:
            import traceback
            print(f"[BOT] Erro ao conectar Discord: {e}")
            traceback.print_exc()
        print("[BOT] Desconectou, reconectando em 15s...")
        import time as _time
        _time.sleep(15)
