# CapsLock Status v1.0.0 (Port Nativo e Ultra-Leve para Linux)

O **CapsLock Status** é um utilitário nativo para Linux desenvolvido em C99 para exibir o estado das teclas **Caps Lock**, **Num Lock** e **Scroll Lock**, projetado para consumo irrisório de recursos (CPU 0.00%, ~2.9 MB de RAM) e compatibilidade total até mesmo com hardware modesto e antigo.

Baseado no projeto original [CapsLockIndicator](https://github.com/jonaskohl/CapsLockIndicator) de Jonas Kohl.

---

## ✨ Novidades e Recursos da Versão 1.0.0

- 🎨 **Ícone Inédito em Vetor SVG**: Desenvolvido sob medida no estilo moderno do Linux Mint, representando a tecla de bloqueio com LED de status verde esmeralda (`data/capslock-status.svg`).
- 🟢 **Design Moderno Linux Mint (2026)**:
  - Janela de configurações com cartões escuros (`#22262C`), destaques em Verde Mint (`#2EA44F`) e tipografia moderna.
  - Abas limpas (*Geral*, *Avisos em Tela*, *Opções Avançadas*).
  - Remoção total de avisos de copyright antigos e verificadores de atualização desnecessários.
- 🎬 **Animação Fiel de Fade Out**:
  - Medida e replicada diretamente do algoritmo original em C#: o aviso em tela permanece 50% do tempo em opacidade total (100% visível) e, nos 50% finais, realiza uma transição suave e gradual de fade out via `_NET_WM_WINDOW_OPACITY` a 50 fps.
- 📐 **Menu de Contexto do Tray Modernizado**:
  - Menu escuro elegante na bandeja do sistema com cantos limpos, divisória sutil e barra de destaque Mint Green / Vermelho no hover ao passar o cursor sobre **Configurações** ou **Sair**.
- 🛡️ **Bandeja Segura (Zero Crashes)**:
  - Encaixe nativo na bandeja via protocolo XEmbed estritamente seguro, sem criação de janelas soltas e sem risco de travamento do Cinnamon / Muffin.

---

## ⚡ Comparativo de Performance

| Métrica | CapsLock Status (C99) | Apps em Python / Electron / Web |
| :--- | :--- | :--- |
| **Uso de CPU Ocioso** | **0.00%** (bloqueio puro no socket do kernel) | 0.5% – 2.0% |
| **Memória RAM Daemon (RSS)** | **~2.9 MB** | 45 MB – 180 MB |
| **Tamanho do Executável** | **47 KB** (C99 com `strip`) | 50 MB – 200 MB |
| **Dependências em Execução** | **Apenas `libX11`** | Runtimes e navegadores pesados |

---

## 🛠️ Compilação e Instalação

Para compilar e otimizar:
```bash
make clean && make strip
```

Para instalar no sistema (`/usr/local/bin`):
```bash
sudo make install
```
*(Também cria atalhos de compatibilidade para `capslock-indicator`)*.

---

## 🚀 Como Usar

### 1. Iniciar o daemon com os ícones na bandeja:
```bash
capslock-status -d -t
```

### 2. Abrir a Janela de Configurações (Interface Gráfica):
```bash
capslock-status -g
# ou diretamente:
capslock-status-gui
```
*(Ou basta dar um clique com o botão esquerdo no ícone da bandeja!)*

### 3. Testar a Animação do Aviso Visual na Tela:
```bash
capslock-status --test
```

### 4. Configurar Inicialização Automática com a Sessão:
```bash
capslock-status --install-autostart
```

---

<small>Port para Linux baseado no projeto original CapsLockIndicator de Jonas Kohl.</small>
