# Passo a passo: publicar no GitHub

> ⚠️ **Antes de tudo:** seus arquivos originais tinham o **token do bot do Telegram** e a **senha do Wi-Fi** escritos no código. Nesta versão eles foram removidos (ficam em `secrets.h`, ignorado pelo Git). Como o token já circulou em arquivos/zip, **revogue-o**: no Telegram, @BotFather → `/revoke` (escolha o bot) → copie o novo token para o `secrets.h`. Nunca faça commit de `secrets.h`.

## 1. Criar o repositório (um integrante só)
1. Entre em github.com → **New repository**.
2. Nome: `decfall-ciberfisico`. Marque *Public* (ou Private e adicione o professor como colaborador).
3. **Não** marque README/.gitignore (já existem aqui). **Create repository**.

## 2. Subir o projeto (terminal na pasta do projeto)
```bash
git init
git branch -M main
git add .
git status            # confira que secrets.h NÃO aparece
git commit -m "feat: estrutura inicial do projeto DECFALL"
git remote add origin https://github.com/SEU_USUARIO/decfall-ciberfisico.git
git push -u origin main
```

## 3. Commits frequentes e descritivos
Faça commits pequenos, por funcionalidade. Exemplos para este projeto:
```bash
git commit -m "feat: adiciona leitura do MPU6050 via I2C"
git commit -m "feat: adiciona rota HTTP /api/dados"
git commit -m "feat: envio de alertas pelo Telegram"
git commit -m "docs: adiciona diagrama de arquitetura"
git commit -m "fix: corrige debounce do botão"
```

## 4. Todos os membros precisam ter commits
1. O dono do repositório: **Settings → Collaborators → Add people** (usuário GitHub de cada colega).
2. Cada colega:
```bash
git clone https://github.com/SEU_USUARIO/decfall-ciberfisico.git
cd decfall-ciberfisico
# edite algo (ex.: preencha nome/RA no README.md)
git add .
git commit -m "docs: adiciona integrante ao README"
git pull --rebase origin main
git push
```
Dica: configure nome/e-mail corretos para o commit aparecer no perfil de cada um:
`git config --global user.name "Seu Nome"` e `git config --global user.email "email-do-github"`.

## 5. Antes de entregar
- [ ] Nomes completos e RAs no README
- [ ] Link do vídeo e screenshots no README
- [ ] `secrets.h` não está no repositório (`git ls-files | grep secrets` deve mostrar só o `.example`)
- [ ] Todos os membros aparecem em *Insights → Contributors*
