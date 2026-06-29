import discord
from discord.ui import Modal, TextInput

ROSA = 0xDB00A6
BANNER = "https://i.imgur.com/DSxv3VU.png"

PLANOS_OPCOES = [
    discord.SelectOption(label="Basic - R$50", description="15 dias de acesso", value="basic", emoji="\U0001f539"),
    discord.SelectOption(label="Diario - R$15", description="1 dia de acesso", value="diario", emoji="\U0001f4a5"),
    discord.SelectOption(label="Semanal - R$50", description="7 dias de acesso", value="semanal", emoji="\U0001f550"),
    discord.SelectOption(label="Mensal - R$100", description="30 dias de acesso", value="mensal", emoji="\U0001f4c5"),
    discord.SelectOption(label="Lifetime - R$500", description="Acesso vitalicio", value="lifetime", emoji="\u26a1"),
]

class MainPanel(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Comprar", style=discord.ButtonStyle.success, custom_id="main_adquirir", emoji="\U0001f6d2")
    async def adquirir(self, i: discord.Interaction, b: discord.ui.Button):
        await i.response.send_message("Selecione o plano desejado:", view=PlanSelect(), ephemeral=True)

class PlanSelect(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.select(placeholder="Escolha seu plano...", options=PLANOS_OPCOES)
    async def select_plan(self, i: discord.Interaction, s: discord.ui.Select):
        from auth_bot import PLANOS, iniciar_checkout
        plan_key = s.values[0]
        info = PLANOS.get(plan_key)
        if not info:
            return await i.response.send_message("Plano invalido.", ephemeral=True)
        await iniciar_checkout(i, plan_key, info)

class ComprovanteModal(Modal, title="Enviar Comprovante"):
    def __init__(self, txid: str, plan: str):
        super().__init__()
        self.txid = txid
        self.plan = plan
        self.add_item(TextInput(
            label="Link do comprovante",
            placeholder="Cole o link da imagem (imgur, discord, etc)",
            style=discord.TextStyle.short,
            required=True,
        ))

    async def on_submit(self, i: discord.Interaction):
        from auth_bot import CONFIG, bot
        link = self.children[0].value.strip()
        embed_comprovante = discord.Embed(
            title="\U0001f4e9 Comprovante Recebido",
            color=discord.Color.green(),
            description="Seu comprovante foi enviado para aprova\u00e7\u00e3o. Aguarde.",
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

    @discord.ui.button(label="Ja paguei!", style=discord.ButtonStyle.success, custom_id="ja_paguei_btn", emoji="\u2705")
    async def ja_paguei(self, i: discord.Interaction, b: discord.ui.Button):
        modal = ComprovanteModal(self.txid, self.plan)
        await i.response.send_modal(modal)

class AdminApproveView(discord.ui.View):
    def __init__(self, txid: str = "", plan: str = "", user_id: int = 0):
        super().__init__(timeout=None)
        self.txid = txid
        self.plan = plan
        self.user_id = user_id

    @discord.ui.button(label="Aprovar", style=discord.ButtonStyle.success, custom_id="admin_approve_btn", emoji="\u2705")
    async def approve_btn(self, i: discord.Interaction, b: discord.ui.Button):
        from auth_bot import CONFIG, aprovar_pagamento
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

    @discord.ui.button(label="Negar", style=discord.ButtonStyle.danger, custom_id="admin_deny_btn", emoji="\u274c")
    async def deny_btn(self, i: discord.Interaction, b: discord.ui.Button):
        from auth_bot import CONFIG, db_exec, db_commit, using_pg
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

class SuporteClose(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Fechar Ticket", style=discord.ButtonStyle.danger, custom_id="suporte_close_btn", emoji="\U0001f512")
    async def close(self, i: discord.Interaction, b: discord.ui.Button):
        from auth_bot import is_admin
        if not is_admin(i):
            return await i.response.send_message("Apenas administradores podem fechar tickets.", ephemeral=True)
        view = SuporteConfirmClose()
        await i.response.send_message("Tem certeza que deseja fechar este ticket?", view=view, ephemeral=True)

class SuporteConfirmClose(discord.ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Sim, fechar", style=discord.ButtonStyle.danger, custom_id="confirm_close_btn", emoji="\u2705")
    async def confirm(self, i: discord.Interaction, b: discord.ui.Button):
        from auth_bot import db_exec, db_commit, db_execone, using_pg
        channel = i.channel
        sql = "SELECT id FROM tickets WHERE channel_id = %s AND status = 'open'" if using_pg else "SELECT id FROM tickets WHERE channel_id = ? AND status = 'open'"
        row = db_execone(sql, (str(channel.id),))
        tid = row["id"] if row and isinstance(row, dict) else (row[0] if row else None)
        if tid:
            usql = "UPDATE tickets SET status = 'closed' WHERE id = %s" if using_pg else "UPDATE tickets SET status = 'closed' WHERE id = ?"
            db_exec(usql, (tid,))
            if not using_pg:
                db_commit()
        await i.response.send_message("Fechando ticket em 5 segundos...")
        await discord.utils.sleep(5)
        try:
            await channel.delete()
        except Exception as e:
            try:
                await channel.send(f"Erro ao deletar: {e}. Feche manualmente.")
            except:
                pass

    @discord.ui.button(label="Cancelar", style=discord.ButtonStyle.secondary, custom_id="cancel_close_btn", emoji="\u274c")
    async def cancel(self, i: discord.Interaction, b: discord.ui.Button):
        await i.response.send_message("Fechamento cancelado.", ephemeral=True)
