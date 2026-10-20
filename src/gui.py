#!/usr/bin/env python3
"""
CapsLock Status - Settings GUI & Native Context Menu (Linux Port)
Modern Linux Mint Edition
Based on Jonas Kohl's CapsLockIndicator
"""

import os
import sys
import configparser
import subprocess
import signal

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GdkPixbuf, GLib

CONFIG_DIR = os.path.expanduser("~/.config/capslock-status")
LEGACY_CONFIG_DIR = os.path.expanduser("~/.config/capslock-indicator")
CONFIG_PATH = os.path.join(CONFIG_DIR, "config.ini")
LEGACY_CONFIG_PATH = os.path.join(LEGACY_CONFIG_DIR, "config.ini")

def get_active_config_path():
    if os.path.exists(CONFIG_PATH):
        return CONFIG_PATH
    if os.path.exists(LEGACY_CONFIG_PATH):
        return LEGACY_CONFIG_PATH
    return CONFIG_PATH

LOGO_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "logo.png")
if not os.path.exists(LOGO_PATH):
    LOGO_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "logo.png")

def get_daemon_bin():
    for d in [os.path.dirname(os.path.abspath(__file__)),
              os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
              os.getcwd()]:
        for name in ["capslock-status", "capslock-indicator"]:
            cand = os.path.join(d, name)
            if os.path.exists(cand) and os.access(cand, os.X_OK):
                return cand
    return "capslock-status"

TRANSLATIONS = {
    "pt-BR": {
        "name": "Português (Brasil)",
        "window_title": "CapsLock Status",
        "tab_general": "Geral",
        "tab_notification": "Avisos em Tela",
        "tab_advanced": "Opções Avançadas",
        "card_icons_title": "ÍCONES NA BANDEJA",
        "cb_icon_num": "Num Lock  [ N ]",
        "cb_icon_caps": "Caps Lock  [ C ]",
        "cb_icon_scroll": "Scroll Lock  [ S ]",
        "card_notify_title": "AVISOS VISUAIS EM TELA",
        "cb_notif_num": "Quando alternar Num Lock",
        "cb_notif_caps": "Quando alternar Caps Lock",
        "cb_notif_scroll": "Quando alternar Scroll Lock",
        "cb_autostart": "Iniciar automaticamente com a sessão",
        "cb_hide_start": "Iniciar minimizado na bandeja",
        "lbl_lang": "Idioma:",
        "btn_hide": "Ocultar Janela",
        "btn_exit": "Encerrar",
        "card_time_title": "TEMPO DE EXIBIÇÃO & ANIMAÇÃO",
        "lbl_time_desc": "O indicador permanece 50% do tempo em opacidade total e realiza um fade out suave nos 50% finais.",
        "card_pos_title": "POSIÇÃO DO AVISO NA TELA",
        "pos_top-left": "Superior Esquerdo",
        "pos_top-center": "Superior Centro",
        "pos_top-right": "Superior Direito",
        "pos_middle-left": "Centro Esquerdo",
        "pos_center": "Centro da Tela",
        "pos_middle-right": "Centro Direito",
        "pos_bottom-left": "Inferior Esquerdo",
        "pos_bottom-center": "Inferior Centro",
        "pos_bottom-right": "Inferior Direito (Padrão)",
        "btn_test": "Testar Aviso na Tela (Fade In / Fade Out)",
        "card_tray_title": "BANDEJA INTELIGENTE (SMART TRAY)",
        "cb_smart_tray": "Ocultar ícones da barra quando a respectiva tecla estiver inativa",
        "card_pers_title": "AVISO FIXO NA TELA (PERSISTENT OVERLAY)",
        "lbl_key": "Tecla",
        "lbl_on": "Fixar quando ativada",
        "lbl_off": "Fixar quando desativada",
        "key_num": "Num Lock",
        "key_caps": "Caps Lock",
        "key_scroll": "Scroll Lock",
        "cb_dismiss": "Fechar aviso imediatamente ao clicar nele com o mouse",
        "footer_subtitle": "Port para Linux baseado no projeto original CapsLockIndicator de Jonas Kohl",
        "menu_settings": "Configurações...",
        "menu_quit": "Sair",
        "strings": {
            "text_caps_on": "Caps lock ativado",
            "text_caps_off": "Caps lock desativado",
            "text_num_on": "Num lock ativado",
            "text_num_off": "Num lock desativado",
            "text_scroll_on": "Scroll lock ativado",
            "text_scroll_off": "Scroll lock desativado",
        }
    },
    "en-GB": {
        "name": "English",
        "window_title": "CapsLock Status",
        "tab_general": "General",
        "tab_notification": "Notification Bubble",
        "tab_advanced": "Advanced Options",
        "card_icons_title": "ICONS",
        "cb_icon_num": "Num Lock  [ N ]",
        "cb_icon_caps": "Caps Lock  [ C ]",
        "cb_icon_scroll": "Scroll Lock  [ S ]",
        "card_notify_title": "NOTIFICATION BUBBLE",
        "cb_notif_num": "Show when Num Lock changes",
        "cb_notif_caps": "Show when Caps Lock changes",
        "cb_notif_scroll": "Show when Scroll Lock changes",
        "cb_autostart": "Start automatically with session",
        "cb_hide_start": "Hide window on startup",
        "lbl_lang": "Language:",
        "btn_hide": "Hide Window",
        "btn_exit": "Exit",
        "card_time_title": "DISPLAY TIME & ANIMATION",
        "lbl_time_desc": "The indicator stays at full opacity for 50% of the duration and smoothly fades out over the remaining 50%.",
        "card_pos_title": "NOTIFICATION POSITION",
        "pos_top-left": "Top Left",
        "pos_top-center": "Top Center",
        "pos_top-right": "Top Right",
        "pos_middle-left": "Middle Left",
        "pos_center": "Screen Center",
        "pos_middle-right": "Middle Right",
        "pos_bottom-left": "Bottom Left",
        "pos_bottom-center": "Bottom Center",
        "pos_bottom-right": "Bottom Right (Default)",
        "btn_test": "Test Notification Display (Fade In / Fade Out)",
        "card_tray_title": "SMART TRAY ICONS",
        "cb_smart_tray": "Hide tray icons when the respective key is inactive",
        "card_pers_title": "PERSISTENT OVERLAY",
        "lbl_key": "Key",
        "lbl_on": "Show when active",
        "lbl_off": "Show when inactive",
        "key_num": "Num Lock",
        "key_caps": "Caps Lock",
        "key_scroll": "Scroll Lock",
        "cb_dismiss": "Dismiss overlay immediately when clicked with mouse",
        "footer_subtitle": "Linux port based on the original CapsLockIndicator by Jonas Kohl",
        "menu_settings": "Settings...",
        "menu_quit": "Quit",
        "strings": {
            "text_caps_on": "Caps lock is on",
            "text_caps_off": "Caps lock is off",
            "text_num_on": "Num lock is on",
            "text_num_off": "Num lock is off",
            "text_scroll_on": "Scroll lock is on",
            "text_scroll_off": "Scroll lock is off",
        }
    },
    "es": {
        "name": "Español",
        "window_title": "CapsLock Status",
        "tab_general": "General",
        "tab_notification": "Avisos en Pantalla",
        "tab_advanced": "Opciones Avanzadas",
        "card_icons_title": "ICONOS EN BANDEJA",
        "cb_icon_num": "Bloq Núm  [ N ]",
        "cb_icon_caps": "Bloq Mayús  [ C ]",
        "cb_icon_scroll": "Bloq Despl  [ S ]",
        "card_notify_title": "AVISOS VISUALES EN PANTALLA",
        "cb_notif_num": "Al alternar Bloq Núm",
        "cb_notif_caps": "Al alternar Bloq Mayús",
        "cb_notif_scroll": "Al alternar Bloq Despl",
        "cb_autostart": "Iniciar automáticamente con la sesión",
        "cb_hide_start": "Iniciar minimizado en la bandeja",
        "lbl_lang": "Idioma:",
        "btn_hide": "Ocultar Ventana",
        "btn_exit": "Salir",
        "card_time_title": "TIEMPO DE VISUALIZACIÓN & ANIMACIÓN",
        "lbl_time_desc": "El indicador permanece al 100% de opacidad la primera mitad del tiempo y se desvanece suavemente al final.",
        "card_pos_title": "POSICIÓN DEL AVISO EN PANTALLA",
        "pos_top-left": "Superior Izquierda",
        "pos_top-center": "Superior Centro",
        "pos_top-right": "Superior Derecha",
        "pos_middle-left": "Centro Izquierda",
        "pos_center": "Centro de Pantalla",
        "pos_middle-right": "Centro Derecha",
        "pos_bottom-left": "Inferior Izquierda",
        "pos_bottom-center": "Inferior Centro",
        "pos_bottom-right": "Inferior Derecha (Por defecto)",
        "btn_test": "Probar Aviso en Pantalla (Fade In / Fade Out)",
        "card_tray_title": "BANDEJA INTELIGENTE (SMART TRAY)",
        "cb_smart_tray": "Ocultar iconos de la bandeja si la tecla está inactiva",
        "card_pers_title": "AVISO FIJO EN PANTALLA (PERSISTENT OVERLAY)",
        "lbl_key": "Tecla",
        "lbl_on": "Fijar si está activada",
        "lbl_off": "Fijar si está desactivada",
        "key_num": "Números",
        "key_caps": "Mayúsculas",
        "key_scroll": "Bloq. Despl",
        "cb_dismiss": "Cerrar aviso inmediatamente al hacer clic con el ratón",
        "footer_subtitle": "Port para Linux basado en el proyecto original CapsLockIndicator de Jonas Kohl",
        "menu_settings": "Configuración...",
        "menu_quit": "Salir",
        "strings": {
            "text_caps_on": "Mayúsculas activado",
            "text_caps_off": "Mayúsculas desactivado",
            "text_num_on": "Números activado",
            "text_num_off": "Números desactivado",
            "text_scroll_on": "Bloq. Despl activado",
            "text_scroll_off": "Bloq. Despl desactivado",
        }
    },
    "fr": {
        "name": "Français",
        "window_title": "CapsLock Status",
        "tab_general": "Général",
        "tab_notification": "Notifications à l'écran",
        "tab_advanced": "Options Avancées",
        "card_icons_title": "ICÔNES DE NOTIFICATION",
        "cb_icon_num": "Verr. Num  [ N ]",
        "cb_icon_caps": "Verr. Maj  [ C ]",
        "cb_icon_scroll": "Arrêt défil.  [ S ]",
        "card_notify_title": "NOTIFICATIONS VISUELLES",
        "cb_notif_num": "Lors du basculement Verr. Num",
        "cb_notif_caps": "Lors du basculement Verr. Maj",
        "cb_notif_scroll": "Lors du basculement Arrêt défil.",
        "cb_autostart": "Lancer automatiquement au démarrage",
        "cb_hide_start": "Démarrer réduit dans la zone de notification",
        "lbl_lang": "Langue :",
        "btn_hide": "Masquer la fenêtre",
        "btn_exit": "Quitter",
        "card_time_title": "DURÉE D'AFFICHAGE & ANIMATION",
        "lbl_time_desc": "L'indicateur reste opaque pendant 50% de la durée puis disparaît en fondu progressif.",
        "card_pos_title": "POSITION DE LA NOTIFICATION",
        "pos_top-left": "En haut à gauche",
        "pos_top-center": "En haut au centre",
        "pos_top-right": "En haut à droite",
        "pos_middle-left": "Au milieu à gauche",
        "pos_center": "Au centre de l'écran",
        "pos_middle-right": "Au milieu à droite",
        "pos_bottom-left": "En bas à gauche",
        "pos_bottom-center": "En bas au centre",
        "pos_bottom-right": "En bas à droite (Défaut)",
        "btn_test": "Tester la notification (Fondu entrant / sortant)",
        "card_tray_title": "ZONE DE NOTIFICATION INTELLIGENTE",
        "cb_smart_tray": "Masquer les icônes lorsque la touche est inactive",
        "card_pers_title": "NOTIFICATION PERSISTANTE",
        "lbl_key": "Touche",
        "lbl_on": "Fixer si active",
        "lbl_off": "Fixer si inactive",
        "key_num": "Verrouillage numérique",
        "key_caps": "Verrouillage majuscules",
        "key_scroll": "Arrêt défil.",
        "cb_dismiss": "Fermer la notification au clic de souris",
        "footer_subtitle": "Portage Linux basé sur le projet original CapsLockIndicator de Jonas Kohl",
        "menu_settings": "Paramètres...",
        "menu_quit": "Quitter",
        "strings": {
            "text_caps_on": "Verrouillage majuscules actif",
            "text_caps_off": "Verrouillage majuscules inactif",
            "text_num_on": "Verrouillage numérique actif",
            "text_num_off": "Verrouillage numérique inactif",
            "text_scroll_on": "Arrêt défil. actif",
            "text_scroll_off": "Arrêt défil. inactif",
        }
    },
    "de": {
        "name": "Deutsch",
        "window_title": "CapsLock Status",
        "tab_general": "Allgemein",
        "tab_notification": "Bildschirmanzeige",
        "tab_advanced": "Erweiterte Optionen",
        "card_icons_title": "INFOBEREICH-SYMBOLE",
        "cb_icon_num": "Num-Lock  [ N ]",
        "cb_icon_caps": "Feststelltaste  [ C ]",
        "cb_icon_scroll": "Rollen-Taste  [ S ]",
        "card_notify_title": "VISUELLE BENACHRICHTIGUNG",
        "cb_notif_num": "Beim Umschalten von Num-Lock",
        "cb_notif_caps": "Beim Umschalten von Feststelltaste",
        "cb_notif_scroll": "Beim Umschalten von Rollen",
        "cb_autostart": "Automatisch mit Sitzung starten",
        "cb_hide_start": "Minimiert im Infobereich starten",
        "lbl_lang": "Sprache:",
        "btn_hide": "Fenster ausblenden",
        "btn_exit": "Beenden",
        "card_time_title": "ANZEIGEDAUER & ANIMATION",
        "lbl_time_desc": "Die Anzeige bleibt 50% der Dauer voll sichtbar und blendet in den restlichen 50% sanft aus.",
        "card_pos_title": "POSITION DER BENACHRICHTIGUNG",
        "pos_top-left": "Oben links",
        "pos_top-center": "Oben Mitte",
        "pos_top-right": "Oben rechts",
        "pos_middle-left": "Mitte links",
        "pos_center": "Bildschirmmitte",
        "pos_middle-right": "Mitte rechts",
        "pos_bottom-left": "Unten links",
        "pos_bottom-center": "Unten Mitte",
        "pos_bottom-right": "Unten rechts (Standard)",
        "btn_test": "Bildschirmanzeige testen (Fade In / Fade Out)",
        "card_tray_title": "INTELLIGENTE SYMBOLE (SMART TRAY)",
        "cb_smart_tray": "Symbole ausblenden, wenn Taste inaktiv ist",
        "card_pers_title": "PERMANENTE BILDSCHIRMANZEIGE",
        "lbl_key": "Taste",
        "lbl_on": "Aktiviert anzeigen",
        "lbl_off": "Deaktiviert anzeigen",
        "key_num": "Num-Lock",
        "key_caps": "Feststelltaste",
        "key_scroll": "Rollen",
        "cb_dismiss": "Anzeige bei Mausklick sofort schließen",
        "footer_subtitle": "Linux-Port basierend auf dem Originalprojekt CapsLockIndicator von Jonas Kohl",
        "menu_settings": "Einstellungen...",
        "menu_quit": "Beenden",
        "strings": {
            "text_caps_on": "Feststelltaste ist an",
            "text_caps_off": "Feststelltaste ist aus",
            "text_num_on": "Num-Lock ist an",
            "text_num_off": "Num-Lock ist aus",
            "text_scroll_on": "Rollen ist an",
            "text_scroll_off": "Rollen ist aus",
        }
    },
    "it": {
        "name": "Italiano",
        "window_title": "CapsLock Status",
        "tab_general": "Generale",
        "tab_notification": "Notifiche a Schermo",
        "tab_advanced": "Opzioni Avanzate",
        "card_icons_title": "ICONE NELL'AREA DI NOTIFICA",
        "cb_icon_num": "Blocco Num  [ N ]",
        "cb_icon_caps": "Blocco Maiusc  [ C ]",
        "cb_icon_scroll": "Blocco Scorr  [ S ]",
        "card_notify_title": "NOTIFICHE VISIVE A SCHERMO",
        "cb_notif_num": "Quando si commuta Blocco Num",
        "cb_notif_caps": "Quando si commuta Blocco Maiusc",
        "cb_notif_scroll": "Quando si commuta Blocco Scorr",
        "cb_autostart": "Avvia automaticamente con la sessione",
        "cb_hide_start": "Avvia minimizzato nell'area di notifica",
        "lbl_lang": "Lingua:",
        "btn_hide": "Nascondi Finestra",
        "btn_exit": "Esci",
        "card_time_title": "DURATA VISUALIZZAZIONE & ANIMAZIONE",
        "lbl_time_desc": "L'indicatore rimane a opacità piena per il 50% del tempo e sfuma delicatamente nella seconda metà.",
        "card_pos_title": "POSIZIONE DELLA NOTIFICA",
        "pos_top-left": "In alto a sinistra",
        "pos_top-center": "In alto al centro",
        "pos_top-right": "In alto a destra",
        "pos_middle-left": "Al centro a sinistra",
        "pos_center": "Centro schermo",
        "pos_middle-right": "Al centro a destra",
        "pos_bottom-left": "In basso a sinistra",
        "pos_bottom-center": "In basso al centro",
        "pos_bottom-right": "In basso a destra (Predefinito)",
        "btn_test": "Prova Notifica a Schermo (Fade In / Fade Out)",
        "card_tray_title": "ICONE INTELLIGENTI (SMART TRAY)",
        "cb_smart_tray": "Nascondi icone se il relativo tasto è disattivato",
        "card_pers_title": "NOTIFICA PERSISTENTE",
        "lbl_key": "Tasto",
        "lbl_on": "Fissa quando attivo",
        "lbl_off": "Fissa quando disattivo",
        "key_num": "Blocco numerico",
        "key_caps": "Blocco maiuscole",
        "key_scroll": "Blocco scorrimento",
        "cb_dismiss": "Chiudi la notifica al clic del mouse",
        "footer_subtitle": "Port per Linux basato sul projeto originale CapsLockIndicator di Jonas Kohl",
        "menu_settings": "Impostazioni...",
        "menu_quit": "Esci",
        "strings": {
            "text_caps_on": "Blocco maiuscole è attivato",
            "text_caps_off": "Blocco maiuscole è disattivato",
            "text_num_on": "Blocco numerico è attivato",
            "text_num_off": "Blocco numerico è disattivato",
            "text_scroll_on": "Blocco scorrimento è attivato",
            "text_scroll_off": "Blocco scorrimento è disattivato",
        }
    }
}

CSS_DATA = b"""
/* Adaptive Linux Mint Theme Styling */
window, .background {
    background-color: @theme_bg_color;
    color: @theme_fg_color;
}

.mint-card {
    background-color: alpha(@theme_fg_color, 0.04);
    border: 1px solid alpha(@theme_fg_color, 0.12);
    border-radius: 8px;
    padding: 14px;
    margin-bottom: 10px;
}

.mint-card-title {
    font-weight: bold;
    font-size: 12px;
    color: @theme_selected_bg_color;
    margin-bottom: 6px;
}

.mint-label-desc {
    opacity: 0.75;
    font-size: 11px;
}

.footer-box {
    background-color: alpha(@theme_fg_color, 0.03);
    border-top: 1px solid alpha(@theme_fg_color, 0.1);
    padding: 10px 16px;
}

.footer-title {
    font-weight: bold;
    font-size: 13px;
}

.footer-subtitle {
    opacity: 0.7;
    font-size: 11px;
}
"""

class CapsLockSettingsWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="CapsLock Status")
        self.set_default_size(630, 440)
        self.set_resizable(False)
        self.set_position(Gtk.WindowPosition.CENTER)

        # Apply subtle adaptive theme-matching CSS
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(CSS_DATA)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        self.config = configparser.ConfigParser()
        self.config_path = get_active_config_path()
        self.load_config()

        self.curr_lang = self.config.get("general", "language", fallback="pt-BR")
        if self.curr_lang not in TRANSLATIONS:
            self.curr_lang = "pt-BR"

        # Set Window Icon
        if os.path.exists(LOGO_PATH):
            self.set_icon_from_file(LOGO_PATH)

        # Main vertical box
        main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(main_vbox)

        # Notebook (Tabs: Geral, Avisos em Tela, Avançado)
        self.notebook = Gtk.Notebook()
        main_vbox.pack_start(self.notebook, True, True, 0)

        # Tab 1: Geral
        tab_general = self.create_general_tab()
        self.lbl_tab_general = Gtk.Label(label="Geral")
        self.notebook.append_page(tab_general, self.lbl_tab_general)

        # Tab 2: Avisos em Tela (Notification)
        tab_notification = self.create_notification_tab()
        self.lbl_tab_notification = Gtk.Label(label="Avisos em Tela")
        self.notebook.append_page(tab_notification, self.lbl_tab_notification)

        # Tab 3: Avançado (Advanced Options)
        tab_advanced = self.create_advanced_tab()
        self.lbl_tab_advanced = Gtk.Label(label="Opções Avançadas")
        self.notebook.append_page(tab_advanced, self.lbl_tab_advanced)

        # Footer
        footer = self.create_footer()
        main_vbox.pack_end(footer, False, False, 0)

        # Initialize labels for current language
        self.retranslate_ui(self.curr_lang)

        self.connect("destroy", Gtk.main_quit)

    def load_config(self):
        if not os.path.exists(self.config_path):
            os.makedirs(os.path.dirname(self.config_path), exist_ok=True)
            subprocess.run([get_daemon_bin(), "--status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.config.read(self.config_path)

    def save_and_notify(self):
        os.makedirs(os.path.dirname(self.config_path), exist_ok=True)
        with open(self.config_path, "w") as f:
            self.config.write(f)
        # Send SIGHUP to reload daemon
        subprocess.run(["killall", "-HUP", "capslock-status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["killall", "-HUP", "capslock-indicator"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def create_general_tab(self):
        container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        container.set_border_width(14)

        # Top row: Two group cards side by side
        top_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        container.pack_start(top_hbox, False, False, 0)

        # 1. Left Card: Ícones na Bandeja
        card_icons = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card_icons.get_style_context().add_class("mint-card")

        self.lbl_icons_title = Gtk.Label(label="", xalign=0)
        self.lbl_icons_title.get_style_context().add_class("mint-card-title")
        card_icons.pack_start(self.lbl_icons_title, False, False, 0)

        self.cb_icon_num = Gtk.CheckButton()
        self.cb_icon_num.set_active(self.config.getboolean("icons", "num_lock", fallback=True))
        self.cb_icon_num.connect("toggled", lambda w: self.on_bool_change("icons", "num_lock", w.get_active()))
        card_icons.pack_start(self.cb_icon_num, False, False, 0)

        self.cb_icon_caps = Gtk.CheckButton()
        self.cb_icon_caps.set_active(self.config.getboolean("icons", "caps_lock", fallback=True))
        self.cb_icon_caps.connect("toggled", lambda w: self.on_bool_change("icons", "caps_lock", w.get_active()))
        card_icons.pack_start(self.cb_icon_caps, False, False, 0)

        self.cb_icon_scroll = Gtk.CheckButton()
        self.cb_icon_scroll.set_active(self.config.getboolean("icons", "scroll_lock", fallback=True))
        self.cb_icon_scroll.connect("toggled", lambda w: self.on_bool_change("icons", "scroll_lock", w.get_active()))
        card_icons.pack_start(self.cb_icon_scroll, False, False, 0)

        top_hbox.pack_start(card_icons, True, True, 0)

        # 2. Right Card: Avisos em Tela (OSD)
        card_notify = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card_notify.get_style_context().add_class("mint-card")

        self.lbl_notify_title = Gtk.Label(label="", xalign=0)
        self.lbl_notify_title.get_style_context().add_class("mint-card-title")
        card_notify.pack_start(self.lbl_notify_title, False, False, 0)

        self.cb_notif_num = Gtk.CheckButton()
        self.cb_notif_num.set_active(self.config.getboolean("notifications", "num_lock", fallback=True))
        self.cb_notif_num.connect("toggled", lambda w: self.on_bool_change("notifications", "num_lock", w.get_active()))
        card_notify.pack_start(self.cb_notif_num, False, False, 0)

        self.cb_notif_caps = Gtk.CheckButton()
        self.cb_notif_caps.set_active(self.config.getboolean("notifications", "caps_lock", fallback=True))
        self.cb_notif_caps.connect("toggled", lambda w: self.on_bool_change("notifications", "caps_lock", w.get_active()))
        card_notify.pack_start(self.cb_notif_caps, False, False, 0)

        self.cb_notif_scroll = Gtk.CheckButton()
        self.cb_notif_scroll.set_active(self.config.getboolean("notifications", "scroll_lock", fallback=True))
        self.cb_notif_scroll.connect("toggled", lambda w: self.on_bool_change("notifications", "scroll_lock", w.get_active()))
        card_notify.pack_start(self.cb_notif_scroll, False, False, 0)

        top_hbox.pack_start(card_notify, True, True, 0)

        # Bottom Card: Sistema e Comportamento
        card_system = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=14)
        card_system.get_style_context().add_class("mint-card")
        container.pack_start(card_system, False, False, 0)

        # Left options
        sys_left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card_system.pack_start(sys_left, True, True, 0)

        self.cb_autostart = Gtk.CheckButton()
        self.cb_autostart.set_active(self.config.getboolean("general", "start_on_logon", fallback=True))
        self.cb_autostart.connect("toggled", self.on_autostart_toggle)
        sys_left.pack_start(self.cb_autostart, False, False, 0)

        self.cb_hide_start = Gtk.CheckButton()
        self.cb_hide_start.set_active(self.config.getboolean("general", "hide_on_startup", fallback=True))
        self.cb_hide_start.connect("toggled", lambda w: self.on_bool_change("general", "hide_on_startup", w.get_active()))
        sys_left.pack_start(self.cb_hide_start, False, False, 0)

        # Right actions & Language
        sys_right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card_system.pack_start(sys_right, True, True, 0)

        # Language dropdown
        lang_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.lbl_lang = Gtk.Label(label="", xalign=0)
        lang_box.pack_start(self.lbl_lang, False, False, 0)

        self.lang_combo = Gtk.ComboBoxText()
        for code, ldata in TRANSLATIONS.items():
            self.lang_combo.append(code, ldata["name"])
        self.lang_combo.set_active_id(self.curr_lang)
        self.lang_combo.connect("changed", self.on_language_change)
        lang_box.pack_start(self.lang_combo, True, True, 0)
        sys_right.pack_start(lang_box, False, False, 0)

        # Action Buttons
        btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        sys_right.pack_start(btn_box, False, False, 0)

        self.btn_hide = Gtk.Button()
        self.btn_hide.connect("clicked", lambda w: self.hide_window())
        btn_box.pack_start(self.btn_hide, True, True, 0)

        self.btn_exit = Gtk.Button()
        self.btn_exit.get_style_context().add_class("destructive-action")
        self.btn_exit.connect("clicked", lambda w: self.exit_app())
        btn_box.pack_start(self.btn_exit, True, True, 0)

        return container

    def create_notification_tab(self):
        container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        container.set_border_width(14)

        # Card 1: Duração e Animação
        card_time = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card_time.get_style_context().add_class("mint-card")

        self.lbl_time_title = Gtk.Label(label="", xalign=0)
        self.lbl_time_title.get_style_context().add_class("mint-card-title")
        card_time.pack_start(self.lbl_time_title, False, False, 0)

        self.lbl_time_desc = Gtk.Label(label="", xalign=0)
        self.lbl_time_desc.set_line_wrap(True)
        self.lbl_time_desc.get_style_context().add_class("mint-label-desc")
        card_time.pack_start(self.lbl_time_desc, False, False, 0)

        slider_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        duration = self.config.getint("overlay", "duration_ms", fallback=500)
        slider = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 300, 5000, 100)
        slider.set_value(duration)
        slider.connect("value-changed", self.on_duration_change)
        slider_box.pack_start(slider, True, True, 0)

        self.lbl_time = Gtk.Label(label=f"{duration} ms")
        self.lbl_time.set_width_chars(8)
        slider_box.pack_start(self.lbl_time, False, False, 0)
        card_time.pack_start(slider_box, False, False, 0)

        container.pack_start(card_time, False, False, 0)

        # Card 2: Posição na Tela
        card_pos = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card_pos.get_style_context().add_class("mint-card")

        self.lbl_pos_title = Gtk.Label(label="", xalign=0)
        self.lbl_pos_title.get_style_context().add_class("mint-card-title")
        card_pos.pack_start(self.lbl_pos_title, False, False, 0)

        grid_pos = Gtk.Grid()
        grid_pos.set_row_spacing(8)
        grid_pos.set_column_spacing(16)
        card_pos.pack_start(grid_pos, False, False, 0)

        positions = [
            ("top-left", 0, 0),
            ("top-center", 1, 0),
            ("top-right", 2, 0),
            ("middle-left", 0, 1),
            ("center", 1, 1),
            ("middle-right", 2, 1),
            ("bottom-left", 0, 2),
            ("bottom-center", 1, 2),
            ("bottom-right", 2, 2)
        ]
        curr_pos = self.config.get("overlay", "position", fallback="bottom-right")

        self.rb_positions = {}
        group = None
        for name, col, row in positions:
            rb = Gtk.RadioButton.new_with_label_from_widget(group, "")
            if not group: group = rb
            if name == curr_pos: rb.set_active(True)
            rb.connect("toggled", self.on_pos_change, name)
            grid_pos.attach(rb, col, row, 1, 1)
            self.rb_positions[name] = rb

        container.pack_start(card_pos, False, False, 0)

        # Test Button
        self.btn_test = Gtk.Button()
        self.btn_test.get_style_context().add_class("suggested-action")
        self.btn_test.connect("clicked", lambda w: subprocess.run([get_daemon_bin(), "--test"]))
        container.pack_start(self.btn_test, False, False, 0)

        return container

    def create_advanced_tab(self):
        container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        container.set_border_width(14)

        # Card 1: Bandeja Inteligente (Smart Tray)
        card_tray = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card_tray.get_style_context().add_class("mint-card")

        self.lbl_tray_title = Gtk.Label(label="", xalign=0)
        self.lbl_tray_title.get_style_context().add_class("mint-card-title")
        card_tray.pack_start(self.lbl_tray_title, False, False, 0)

        self.cb_smart_tray = Gtk.CheckButton()
        self.cb_smart_tray.set_active(self.config.getboolean("icons", "tray_only_when_active", fallback=False))
        self.cb_smart_tray.connect("toggled", lambda w: self.on_bool_change("icons", "tray_only_when_active", w.get_active()))
        card_tray.pack_start(self.cb_smart_tray, False, False, 0)

        container.pack_start(card_tray, False, False, 0)

        # Card 2: Indicador Persistente (Sticky Overlay)
        card_adv = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card_adv.get_style_context().add_class("mint-card")

        self.lbl_pers_title = Gtk.Label(label="", xalign=0)
        self.lbl_pers_title.get_style_context().add_class("mint-card-title")
        card_adv.pack_start(self.lbl_pers_title, False, False, 0)

        grid_pers = Gtk.Grid()
        grid_pers.set_row_spacing(6)
        grid_pers.set_column_spacing(24)
        card_adv.pack_start(grid_pers, False, False, 0)

        self.lbl_grid_key = Gtk.Label(label="", xalign=0)
        self.lbl_grid_key.get_style_context().add_class("mint-label-desc")
        grid_pers.attach(self.lbl_grid_key, 0, 0, 1, 1)

        self.lbl_grid_on = Gtk.Label(label="", xalign=0)
        self.lbl_grid_on.get_style_context().add_class("mint-label-desc")
        grid_pers.attach(self.lbl_grid_on, 1, 0, 1, 1)

        self.lbl_grid_off = Gtk.Label(label="", xalign=0)
        self.lbl_grid_off.get_style_context().add_class("mint-label-desc")
        grid_pers.attach(self.lbl_grid_off, 2, 0, 1, 1)

        # Num lock
        self.lbl_key_num = Gtk.Label(label="", xalign=0)
        grid_pers.attach(self.lbl_key_num, 0, 1, 1, 1)
        cb_num_on = Gtk.CheckButton()
        cb_num_on.set_active(self.config.getboolean("overlay", "persistent_num_on", fallback=False))
        cb_num_on.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_num_on", w.get_active()))
        grid_pers.attach(cb_num_on, 1, 1, 1, 1)

        cb_num_off = Gtk.CheckButton()
        cb_num_off.set_active(self.config.getboolean("overlay", "persistent_num_off", fallback=False))
        cb_num_off.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_num_off", w.get_active()))
        grid_pers.attach(cb_num_off, 2, 1, 1, 1)

        # Caps lock
        self.lbl_key_caps = Gtk.Label(label="", xalign=0)
        grid_pers.attach(self.lbl_key_caps, 0, 2, 1, 1)
        cb_caps_on = Gtk.CheckButton()
        cb_caps_on.set_active(self.config.getboolean("overlay", "persistent_caps_on", fallback=False))
        cb_caps_on.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_caps_on", w.get_active()))
        grid_pers.attach(cb_caps_on, 1, 2, 1, 1)

        cb_caps_off = Gtk.CheckButton()
        cb_caps_off.set_active(self.config.getboolean("overlay", "persistent_caps_off", fallback=False))
        cb_caps_off.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_caps_off", w.get_active()))
        grid_pers.attach(cb_caps_off, 2, 2, 1, 1)

        # Scroll lock
        self.lbl_key_scroll = Gtk.Label(label="", xalign=0)
        grid_pers.attach(self.lbl_key_scroll, 0, 3, 1, 1)
        cb_scroll_on = Gtk.CheckButton()
        cb_scroll_on.set_active(self.config.getboolean("overlay", "persistent_scroll_on", fallback=False))
        cb_scroll_on.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_scroll_on", w.get_active()))
        grid_pers.attach(cb_scroll_on, 1, 3, 1, 1)

        cb_scroll_off = Gtk.CheckButton()
        cb_scroll_off.set_active(self.config.getboolean("overlay", "persistent_scroll_off", fallback=False))
        cb_scroll_off.connect("toggled", lambda w: self.on_bool_change("overlay", "persistent_scroll_off", w.get_active()))
        grid_pers.attach(cb_scroll_off, 2, 3, 1, 1)

        self.cb_dismiss = Gtk.CheckButton()
        self.cb_dismiss.set_active(self.config.getboolean("overlay", "dismiss_on_click", fallback=True))
        self.cb_dismiss.connect("toggled", lambda w: self.on_bool_change("overlay", "dismiss_on_click", w.get_active()))
        card_adv.pack_start(self.cb_dismiss, False, False, 4)

        container.pack_start(card_adv, False, False, 0)
        return container

    def create_footer(self):
        footer_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=14)
        footer_box.get_style_context().add_class("footer-box")

        # Logo image
        if os.path.exists(LOGO_PATH):
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(LOGO_PATH, 42, 42, True)
            img = Gtk.Image.new_from_pixbuf(pixbuf)
            footer_box.pack_start(img, False, False, 0)

        # Text labels
        text_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        footer_box.pack_start(text_vbox, True, True, 0)

        lbl_name = Gtk.Label(label="CapsLock Status  v1.0.0")
        lbl_name.get_style_context().add_class("footer-title")
        lbl_name.set_xalign(0)
        text_vbox.pack_start(lbl_name, False, False, 0)

        self.lbl_footer_desc = Gtk.Label(label="")
        self.lbl_footer_desc.get_style_context().add_class("footer-subtitle")
        self.lbl_footer_desc.set_xalign(0)
        text_vbox.pack_start(self.lbl_footer_desc, False, False, 0)

        return footer_box

    def retranslate_ui(self, lang_id):
        t = TRANSLATIONS.get(lang_id, TRANSLATIONS["en-GB"])
        self.set_title(t["window_title"])

        # Tab labels
        self.lbl_tab_general.set_text(t["tab_general"])
        self.lbl_tab_notification.set_text(t["tab_notification"])
        self.lbl_tab_advanced.set_text(t["tab_advanced"])

        # General tab
        self.lbl_icons_title.set_text(t["card_icons_title"])
        self.cb_icon_num.set_label(t["cb_icon_num"])
        self.cb_icon_caps.set_label(t["cb_icon_caps"])
        self.cb_icon_scroll.set_label(t["cb_icon_scroll"])

        self.lbl_notify_title.set_text(t["card_notify_title"])
        self.cb_notif_num.set_label(t["cb_notif_num"])
        self.cb_notif_caps.set_label(t["cb_notif_caps"])
        self.cb_notif_scroll.set_label(t["cb_notif_scroll"])

        self.cb_autostart.set_label(t["cb_autostart"])
        self.cb_hide_start.set_label(t["cb_hide_start"])
        self.lbl_lang.set_text(t["lbl_lang"])
        self.btn_hide.set_label(t["btn_hide"])
        self.btn_exit.set_label(t["btn_exit"])

        # Notification tab
        self.lbl_time_title.set_text(t["card_time_title"])
        self.lbl_time_desc.set_text(t["lbl_time_desc"])
        self.lbl_pos_title.set_text(t["card_pos_title"])
        for pos_key, rb in self.rb_positions.items():
            rb.set_label(t.get(f"pos_{pos_key}", pos_key))
        self.btn_test.set_label(t["btn_test"])

        # Advanced tab
        self.lbl_tray_title.set_text(t["card_tray_title"])
        self.cb_smart_tray.set_label(t["cb_smart_tray"])
        self.lbl_pers_title.set_text(t["card_pers_title"])
        self.lbl_grid_key.set_text(t["lbl_key"])
        self.lbl_grid_on.set_text(t["lbl_on"])
        self.lbl_grid_off.set_text(t["lbl_off"])
        self.lbl_key_num.set_text(t["key_num"])
        self.lbl_key_caps.set_text(t["key_caps"])
        self.lbl_key_scroll.set_text(t["key_scroll"])
        self.cb_dismiss.set_label(t["cb_dismiss"])

        # Footer
        self.lbl_footer_desc.set_text(t["footer_subtitle"])

    def on_bool_change(self, section, key, value):
        if not self.config.has_section(section):
            self.config.add_section(section)
        self.config.set(section, key, "true" if value else "false")
        self.save_and_notify()

    def on_autostart_toggle(self, widget):
        active = widget.get_active()
        self.on_bool_change("general", "start_on_logon", active)
        if active:
            subprocess.run([get_daemon_bin(), "--install-autostart"], stdout=subprocess.DEVNULL)
        else:
            subprocess.run([get_daemon_bin(), "--uninstall-autostart"], stdout=subprocess.DEVNULL)

    def on_language_change(self, combo):
        lang = combo.get_active_id()
        if not lang or lang not in TRANSLATIONS:
            return
        self.curr_lang = lang

        # 1. Update all GUI text instantly
        self.retranslate_ui(lang)

        # 2. Update config file
        if not self.config.has_section("general"):
            self.config.add_section("general")
        self.config.set("general", "language", lang)

        if not self.config.has_section("strings"):
            self.config.add_section("strings")
        t = TRANSLATIONS[lang]
        for k, v in t["strings"].items():
            self.config.set("strings", k, v)

        # 3. Save and signal background daemon
        self.save_and_notify()

    def on_duration_change(self, slider):
        val = int(slider.get_value())
        self.lbl_time.set_text(f"{val} ms")
        if not self.config.has_section("overlay"): self.config.add_section("overlay")
        self.config.set("overlay", "duration_ms", str(val))
        self.save_and_notify()

    def on_pos_change(self, rb, name):
        if rb.get_active():
            if not self.config.has_section("overlay"): self.config.add_section("overlay")
            self.config.set("overlay", "position", name)
            self.save_and_notify()

    def hide_window(self):
        self.hide()
        Gtk.main_quit()

    def exit_app(self):
        # Stop background daemon
        subprocess.run(["killall", "capslock-status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["killall", "capslock-indicator"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        Gtk.main_quit()


def show_tray_context_menu(target_x=None, target_y=None):
    """
    Native Linux Mint tray context menu.
    Draws using the active system theme with clean typography and proper placement
    above the panel workarea.
    """
    config = configparser.ConfigParser()
    config_path = get_active_config_path()
    if os.path.exists(config_path):
        config.read(config_path)
    lang_id = config.get("general", "language", fallback="pt-BR")
    t = TRANSLATIONS.get(lang_id, TRANSLATIONS["en-GB"])

    menu = Gtk.Menu()

    # Settings item
    item_settings = Gtk.MenuItem(label=t["menu_settings"])
    activated_item = [False]
    def on_settings_activate(w):
        activated_item[0] = True
        try:
            with open("/tmp/capslock_click.log", "a") as f:
                f.write("GUI: on_settings_activate called!\n")
        except Exception:
            pass
        win = CapsLockSettingsWindow()
        win.show_all()
        win.present()
    item_settings.connect("activate", on_settings_activate)
    menu.append(item_settings)

    # Separator
    menu.append(Gtk.SeparatorMenuItem())

    # Exit item
    item_exit = Gtk.MenuItem(label=t["menu_quit"])
    def on_exit_activate(w):
        activated_item[0] = True
        try:
            with open("/tmp/capslock_click.log", "a") as f:
                f.write("GUI: on_exit_activate called!\n")
        except Exception:
            pass
        subprocess.run(["killall", "capslock-status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["killall", "capslock-indicator"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        Gtk.main_quit()
    item_exit.connect("activate", on_exit_activate)
    menu.append(item_exit)

    menu.show_all()

    try:
        with open("/tmp/capslock_click.log", "a") as f:
            f.write(f"GUI: show_tray_context_menu called target_x={target_x} target_y={target_y}\n")
    except Exception:
        pass

    def check_dismiss():
        try:
            with open("/tmp/capslock_click.log", "a") as f:
                f.write(f"GUI: check_dismiss activated_item={activated_item[0]}\n")
        except Exception:
            pass
        if not activated_item[0]:
            Gtk.main_quit()
        return False

    def on_deactivate(m):
        GLib.timeout_add(100, check_dismiss)
    menu.connect("deactivate", on_deactivate)

    def pos_func(m, px, py, push_in):
        disp = Gdk.Display.get_default()
        seat = disp.get_default_seat()
        pointer = seat.get_pointer()
        _, cur_x, cur_y = pointer.get_position()
        tx = target_x if (target_x is not None and target_x > 0) else cur_x
        ty = target_y if (target_y is not None and target_y > 0) else cur_y

        mon = disp.get_monitor_at_point(tx, ty) if hasattr(disp, "get_monitor_at_point") else disp.get_monitor(0)
        work = mon.get_workarea() if mon else Gdk.Rectangle()
        if work.height <= 0:
            screen = Gdk.Screen.get_default()
            work.x, work.y, work.width, work.height = 0, 0, screen.get_width(), screen.get_height()

        min_sz, nat_sz = m.get_preferred_size()
        menu_w = nat_sz.width if nat_sz.width > 0 else 160
        menu_h = nat_sz.height if nat_sz.height > 0 else 70

        final_x = tx
        if final_x + menu_w > work.x + work.width:
            final_x = max(work.x, work.x + work.width - menu_w - 6)

        # If click was in the bottom half of the screen/workarea, open UP above the panel
        if ty > work.y + work.height / 2:
            final_y = min(ty, work.y + work.height) - menu_h - 4
            if final_y < work.y:
                final_y = work.y
        else:
            final_y = ty + 4

        return (final_x, final_y, True)

    menu.popup(None, None, pos_func, None, 0, Gtk.get_current_event_time())
    Gtk.main()


def main():
    if "--menu" in sys.argv:
        idx = sys.argv.index("--menu")
        x, y = None, None
        try:
            if idx + 2 < len(sys.argv):
                x = int(sys.argv[idx + 1])
                y = int(sys.argv[idx + 2])
        except (ValueError, IndexError):
            pass
        show_tray_context_menu(x, y)
        return

    # Make sure background daemon is running
    res = subprocess.run(["killall", "-0", "capslock-status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if res.returncode != 0:
        res2 = subprocess.run(["killall", "-0", "capslock-indicator"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if res2.returncode != 0:
            subprocess.Popen([get_daemon_bin(), "-d", "-t"])

    win = CapsLockSettingsWindow()
    win.show_all()
    Gtk.main()

if __name__ == "__main__":
    main()
