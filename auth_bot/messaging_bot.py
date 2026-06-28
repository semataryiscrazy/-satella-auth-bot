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
from datetime import datetime, timezone
from flask import Flask, request, jsonify
import os
import re
import urllib.request
import hmac

script_dir = os.path.dirname(os.path.abspath(__file__))
config_path = os.path.join(script_dir, "messaging_config.json")

CONFIG = {}
if os.path.exists(config_path):
    with open(config_path) as f:
        CONFIG = json.load(f)

CONFIG["token"] = os.environ.get("MESSAGING_TOKEN", CONFIG.get("token", ""))
CONFIG["admin_ids"] = [
    int(x.strip()) for x in os.environ.get("ADMIN_IDS", "").split(",") if x.strip()
] or CONFIG.get("admin_ids", [])
CONFIG["log_channel_id"] = int(os.environ.get("LOG_CHANNEL_ID", CONFIG.get("log_channel_id", 0)))
CONFIG["guild_id"] = int(os.environ.get("GUILD_ID", CONFIG.get("guild_id", 0)))
CONFIG["client_role_id"] = int(os.environ.get("CLIENT_ROLE_ID", CONFIG.get("client_role_id", 0)))
CONFIG["webhook_secret"] = os.environ.get("WEBHOOK_SECRET", CONFIG.get("webhook_secret", ""))
CONFIG["pix_key"] = os.environ.get("PIX_KEY", CONFIG.get("pix_key", ""))
CONFIG["pix_name"] = os.environ.get("PIX_NAME", CONFIG.get("pix_name", "Satella"))

# Database (same as auth_bot)
DATABASE_URL = os.environ.get("DATABASE_URL", CONFIG.get("db_path", os.path.join(script_dir, "auth.db")))

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
    db_path = CONFIG.get("db_path", DATABASE_URL)
    if db_path and str(db_path).startswith("postgres") and PSYCOPG2_AVAILABLE:
        try:
            DB = psycopg2.connect(str(db_path), cursor_factory=RealDictCursor)
            DB.autocommit = True
            using_pg = True
            print("[SALES-DB] Conectado ao PostgreSQL!")
            return DB
        except Exception as e:
            print(f"[SALES-DB] PostgreSQL failed: {e}")
            using_pg = False
    using_pg = False
    DB = sqlite3.connect(str(db_path), check_same_thread=False)
    DB.row_factory = sqlite3.Row
    DB.execute("PRAGMA journal_mode=WAL")
    DB.execute("PRAGMA foreign_keys=ON")
    return DB

def db_exec(sql, params=None):
    db = get_db()
    if using_pg:
        sql = sql.replace("?", "%s")
        sql = re.sub(r'INTEGER PRIMARY KEY AUTOINCREMENT', 'SERIAL PRIMARY KEY', sql, flags=re.IGNORECASE)
        try:
            with db.cursor() as cur:
                cur.execute(sql, params or ())
                try:
                    rows = cur.fetchall()
                    return [dict(r) for r in rows]
                except:
                    return []
        except Exception as e:
            print(f"[SALES-DB] PG error: {e}")
            return []
    else:
        try:
            if params:
                return db.execute(sql, params)
            return db.execute(sql)
        except Exception as e:
            print(f"[SALES-DB] SQLite error: {e}")
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

def now():
    return int(time.time())

def gen_key():
    seg = lambda: ''.join(secrets.choice(string.ascii_uppercase + string.digits) for _ in range(5))
    return f"{seg()}-{seg()}-{seg()}"

PLANOS = {
    "diario":   {"nome": "Diario",   "preco": 15,  "dias": 1,     "emoji": "\U0001f4a5"},
    "semanal":  {"nome": "Semanal",  "preco": 50,  "dias": 7,     "emoji": "\U0001f550"},
    "mensal":   {"nome": "Mensal",   "preco": 100, "dias": 30,    "emoji": "\U0001f4c5"},
    "lifetime": {"nome": "Lifetime", "preco": 500, "dias": 36500, "emoji": "\u26a1"},
    "basic":    {"nome": "Basic",    "preco": 50,  "dias": 15,    "emoji": "\U0001f539"},
}

BANNER = "https://i.imgur.com/DSxv3VU.png"
ROSA = 0xDB00A6

# ========== PIX ==========

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

# ========== DISCORD BOT ==========

intents = discord.Intents.default()
intents.members = True
bot = commands.Bot(command_prefix="!", intents=intents)

def is_admin(i: discord.Interaction):
    return i.user.id in CONFIG["admin_ids"]

async def log_action(text: str):
    cid = CONFIG.get("log_channel_id")
    if cid:
        ch = bot.get_channel(cid)
        if ch:
            try:
                await ch.send(f"\U0001f4dd `{text}`")
            except:
                pass

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
            db_exec("INSERT INTO keys (key, duration_days, created_at) VALUES (%s, %s, %s)",
                    (k, dias, now()))
        else:
            db_exec("INSERT INTO keys (key, duration_days, created_at) VALUES (?, ?, ?)",
                    (k, dias, now()))
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
    await log_action(f"APROVADO: {member} ({member.id}) plano={plan} key={k} cargo={'SIM' if role else 'NAO'}")
    return k

# ========== FLASK / WEBHOOK ==========

app = Flask(__name__)

@app.route("/api/pix/webhook", methods=["POST"])
def pix_webhook():
    data = request.get_json(silent=True) or {}
    secret = request.headers.get("X-Webhook-Secret", "")
    if CONFIG.get("webhook_secret") and secret != CONFIG["webhook_secret"]:
        return jsonify({"success": False, "error": "Invalid secret"}), 401
    txid = data.get("txid") or data.get("id") or data.get("pix", {}).get("txid", "")
    status = (data.get("status") or data.get("action") or "").lower()
    if not txid:
        return jsonify({"success": False, "error": "Missing txid"}), 400
    if status not in ("paid", "completed", "confirmed", "approved"):
        return jsonify({"success": False, "error": f"Status ignorado: {status}"})
    row = db_execone(
        "SELECT * FROM pix_pending WHERE txid = ?" if not using_pg else "SELECT * FROM pix_pending WHERE txid = %s",
        (txid,)
    )
    if not row:
        return jsonify({"success": False, "error": "txid not found"}), 404
    row = dict(row) if not isinstance(row, dict) else row
    if row.get("status") == "paid":
        return jsonify({"success": True, "message": "Already paid"})
    member_id = int(row["user_id"])
    guild = None
    for g in bot.guilds:
        if g.id == CONFIG.get("guild_id", 0) or g.get_member(member_id):
            guild = g
            break
    member = guild.get_member(member_id) if guild else None
    if not member:
        try:
            member = await bot.fetch_user(member_id)
        except:
            member = None
    if not member:
        return jsonify({"success": False, "error": "Member not found"}), 404
    plan = row["plan"]
    result = await aprovar_pagamento(member, plan, txid)
    if not result:
        return jsonify({"success": False, "error": "Failed to generate key"}), 500
    return jsonify({"success": True, "key": result})

@app.route("/api/pix/status", methods=["GET"])
def pix_status():
    txid = request.args.get("txid", "")
    if not txid:
        return jsonify({"success": False, "error": "Missing txid"}), 400
    row = db_execone(
        "SELECT status FROM pix_pending WHERE txid = ?" if not using_pg else "SELECT status FROM pix_pending WHERE txid = %s",
        (txid,)
    )
    if not row:
        return jsonify({"success": False, "error": "Not found"}), 404
    row = dict(row) if not isinstance(row, dict) else row
    return jsonify({"success": True, "status": row["status"]})

# ========== DISCORD COMMANDS ==========

@bot.event
async def on_ready():
    print(f"[SALES] Logado como {bot.user}")
    try:
        if CONFIG.get("guild_id"):
            guild_obj = discord.Object(id=CONFIG["guild_id"])
            bot.tree.copy_global_to(guild=guild_obj)
            synced = await bot.tree.sync(guild=guild_obj)
        else:
            synced = await bot.tree.sync()
        print(f"[SALES] {len(synced)} comandos sincronizados")
    except Exception as e:
        print(f"[SALES] Erro sync: {e}")

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
        discord.SelectOption(label="Basic - R$50", description="15 dias de acesso", value="basic", emoji="\U0001f539"),
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
        "\U0001f539 **Basic** — R$50,00 *(15 dias)*\n"
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
    app_commands.Choice(name="Basic - R$50 (15 dias)", value="basic"),
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

class AdminApproveView(discord.ui.View):
    def __init__(self, txid: str, plan: str, user_id: int):
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

class ComprovanteView(discord.ui.View):
    def __init__(self, txid: str, plan: str):
        super().__init__(timeout=None)
        self.txid = txid
        self.plan = plan

    @discord.ui.button(label="Ja paguei!", style=discord.ButtonStyle.success, emoji="\u2705")
    async def ja_paguei(self, i: discord.Interaction, b: discord.ui.Button):
        modal = ComprovanteModal(self.txid, self.plan)
        await i.response.send_modal(modal)

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

# ========== RUN ==========

def run_flask():
    port = int(os.environ.get("PORT", CONFIG.get("api_port", 5001)))
    app.run(host="0.0.0.0", port=port, debug=False, use_reloader=False)

def run_bot():
    bot.run(CONFIG["token"])

if __name__ == "__main__":
    print("[SALES] Iniciando bot de vendas...")
    # ensure pix_pending table exists
    try:
        if not using_pg:
            db_exec("CREATE TABLE IF NOT EXISTS pix_pending (id INTEGER PRIMARY KEY AUTOINCREMENT, txid TEXT UNIQUE, user_id TEXT, plan TEXT, status TEXT DEFAULT 'pending', created_at INTEGER)")
            db_commit()
    except:
        pass
    t1 = threading.Thread(target=run_flask, daemon=True)
    t1.start()
    print(f"[API] Rodando na porta {os.environ.get('PORT', CONFIG.get('api_port', 5001))}")
    while True:
        try:
            print("[BOT] Conectando ao Discord...")
            run_bot()
        except Exception as e:
            import traceback
            print(f"[BOT] Erro: {e}")
            traceback.print_exc()
        print("[BOT] Desconectou, reconectando em 15s...")
        import time as _time
        _time.sleep(15)
