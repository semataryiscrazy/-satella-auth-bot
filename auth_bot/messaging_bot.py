import discord
from discord import app_commands
from discord.ext import commands
import json
import os

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

intents = discord.Intents.default()
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

@bot.event
async def on_ready():
    print(f"[MESSAGING] Logado como {bot.user}")
    try:
        synced = await bot.tree.sync()
        print(f"[MESSAGING] {len(synced)} comandos sincronizados")
    except Exception as e:
        print(f"[MESSAGING] Erro sync: {e}")

@bot.tree.command(name="say", description="Enviar mensagem em um canal")
@app_commands.describe(canal="Canal para enviar", mensagem="Texto da mensagem")
async def cmd_say(i: discord.Interaction, canal: discord.TextChannel, mensagem: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    await canal.send(mensagem)
    await i.response.send_message(f"\u2705 Mensagem enviada em {canal.mention}", ephemeral=True)
    await log_action(f"{i.user} /say em #{canal.name}: {mensagem[:60]}")

@bot.tree.command(name="embed", description="Enviar embed em um canal")
@app_commands.describe(canal="Canal para enviar", titulo="Titulo do embed", descricao="Descricao do embed", cor="Cor em hex (ex: DB00A6)")
async def cmd_embed(i: discord.Interaction, canal: discord.TextChannel, titulo: str, descricao: str, cor: str = "DB00A6"):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    try:
        color = int(cor.lstrip("#"), 16)
    except:
        color = 0xDB00A6
    embed = discord.Embed(title=titulo, description=descricao, color=color)
    await canal.send(embed=embed)
    await i.response.send_message(f"\u2705 Embed enviado em {canal.mention}", ephemeral=True)
    await log_action(f"{i.user} /embed em #{canal.name}: {titulo[:40]}")

@bot.tree.command(name="dm", description="Enviar DM para um usuario")
@app_commands.describe(usuario="Usuario do Discord", mensagem="Texto da mensagem")
async def cmd_dm(i: discord.Interaction, usuario: discord.User, mensagem: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    try:
        await usuario.send(mensagem)
        await i.response.send_message(f"\u2705 DM enviada para {usuario.mention}", ephemeral=True)
        await log_action(f"{i.user} /dm para {usuario}: {mensagem[:60]}")
    except Exception as e:
        await i.response.send_message(f"\u274c Erro ao enviar DM: {e}", ephemeral=True)

@bot.tree.command(name="dmembed", description="Enviar embed na DM de um usuario")
@app_commands.describe(usuario="Usuario do Discord", titulo="Titulo do embed", descricao="Descricao do embed", cor="Cor em hex (ex: DB00A6)")
async def cmd_dmembed(i: discord.Interaction, usuario: discord.User, titulo: str, descricao: str, cor: str = "DB00A6"):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    try:
        color = int(cor.lstrip("#"), 16)
    except:
        color = 0xDB00A6
    embed = discord.Embed(title=titulo, description=descricao, color=color)
    await usuario.send(embed=embed)
    await i.response.send_message(f"\u2705 Embed enviado para {usuario.mention}", ephemeral=True)
    await log_action(f"{i.user} /dmembed para {usuario}: {titulo[:40]}")

@bot.tree.command(name="announce", description="Anunciar em todos os canais de um cargo")
@app_commands.describe(cargo="Cargo para marcar", mensagem="Texto do anuncio")
async def cmd_announce(i: discord.Interaction, cargo: discord.Role, mensagem: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    embed = discord.Embed(title="\U0001f4e2 Anuncio", description=mensagem, color=0xDB00A6)
    embed.set_footer(text=f"Por {i.user.name}")
    for ch in i.guild.text_channels:
        perms = ch.permissions_for(i.guild.default_role)
        if perms.read_messages and perms.send_messages:
            try:
                await ch.send(content=cargo.mention, embed=embed)
            except:
                pass
    await i.response.send_message(f"\u2705 Anuncio enviado para todos os canais", ephemeral=True)
    await log_action(f"{i.user} /announce: {mensagem[:60]}")

@bot.tree.command(name="edit", description="Editar mensagem do bot")
@app_commands.describe(canal="Canal da mensagem", mensagem_id="ID da mensagem", novo_texto="Novo texto")
async def cmd_edit(i: discord.Interaction, canal: discord.TextChannel, mensagem_id: str, novo_texto: str):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    try:
        msg = await canal.fetch_message(int(mensagem_id))
        await msg.edit(content=novo_texto)
        await i.response.send_message(f"\u2705 Mensagem editada em {canal.mention}", ephemeral=True)
        await log_action(f"{i.user} /edit em #{canal.name} id:{mensagem_id}")
    except Exception as e:
        await i.response.send_message(f"\u274c Erro: {e}", ephemeral=True)

@bot.tree.command(name="purge", description="Apagar mensagens de um canal")
@app_commands.describe(canal="Canal", quantidade="Numero de mensagens (1-100)")
async def cmd_purge(i: discord.Interaction, canal: discord.TextChannel, quantidade: app_commands.Range[int, 1, 100]):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    await canal.purge(limit=quantidade)
    await i.response.send_message(f"\u2705 {quantidade} mensagens apagadas em {canal.mention}", ephemeral=True)
    await log_action(f"{i.user} /purge {quantidade} em #{canal.name}")

@bot.tree.command(name="listcanais", description="Listar canais do servidor")
async def cmd_listcanais(i: discord.Interaction):
    if not is_admin(i):
        return await i.response.send_message("Sem permissão.", ephemeral=True)
    canais = "\n".join([f"{c.mention} - `{c.id}`" for c in i.guild.text_channels[:30]])
    embed = discord.Embed(title="Canais do Servidor", description=canais, color=0xDB00A6)
    await i.response.send_message(embed=embed, ephemeral=True)

if __name__ == "__main__":
    print("[MESSAGING] Iniciando...")
    if not CONFIG.get("token"):
        print("[MESSAGING] Token nao configurado!")
        exit(1)
    bot.run(CONFIG["token"])
