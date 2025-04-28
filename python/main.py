import sys
import glob
import serial
import pyautogui
import tkinter as tk
from tkinter import ttk, messagebox
from time import sleep

pyautogui.PAUSE = 0.0

#----------------------------------------------------------------------#
# Funções de controle
#----------------------------------------------------------------------#
def move_mouse(axis, value):
    """Move o mouse de acordo com o eixo e valor recebidos."""
    if axis == 0:
        pyautogui.moveRel(value, 0)
    elif axis == 1:
        pyautogui.moveRel(0, value)
    elif axis == 2:
        pyautogui.moveRel(value, 0)
    elif axis == 3:
        pyautogui.moveRel(0, value)

def parse_data(data):
    """Interpreta os dados recebidos do buffer (axis + valor)."""
    axis = data[0]
    # valor vem em little-endian, signed
    value = int.from_bytes(data[1:3], byteorder='little', signed=True)
    return axis, value

def serial_ports():
    """Retorna uma lista das portas seriais disponíveis."""
    ports = []
    if sys.platform.startswith('win'):
        for i in range(1, 256):
            port = f'COM{i}'
            try:
                s = serial.Serial(port)
                s.close()
                ports.append(port)
            except (OSError, serial.SerialException):
                pass
    elif sys.platform.startswith(('linux','cygwin')):
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Plataforma não suportada.')
    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except:
            pass
    return result

#----------------------------------------------------------------------#
# Loop principal de leitura e mapeamento
#----------------------------------------------------------------------#
pressed_keys = set()

def controle(ser):
    key_map = {
        4:  'a',     5:  'z',
        6:  's',     7:  'x',
        8:  'enter',
        9:  'up',   10: 'down',
       11: 'left',  12: 'right',
    }
    while True:
        sync = ser.read(1)
        if not sync or sync[0] != 0xFF:
            continue
        raw = ser.read(3)
        if len(raw) < 3:
            continue
        axis, value = parse_data(raw)

        if axis in (0, 1, 2, 3):
            # eixos analógicos (0=X,1=Y,2=X2,3=Y2)
            move_mouse(axis, value)
        elif axis in key_map:
            key = key_map[axis]
            if value == 1 and key not in pressed_keys:
                pyautogui.keyDown(key)
                pressed_keys.add(key)
            elif value == 0 and key in pressed_keys:
                pyautogui.keyUp(key)
                pressed_keys.remove(key)
        # else: ignora outros axis

#----------------------------------------------------------------------#
# GUI Tkinter
#----------------------------------------------------------------------#
def conectar_porta(port_name, root, botao, status_label, mudar_cor):
    if not port_name:
        messagebox.showwarning("Aviso", "Selecione uma porta antes de conectar.")
        return
    try:
        ser = serial.Serial(port_name, 115200, timeout=1)
        status_label.config(text=f"Conectado em {port_name}", foreground="green")
        mudar_cor("green")
        botao.config(text="Desconectar")
        root.update()
        controle(ser)
    except Exception as e:
        messagebox.showerror("Erro de Conexão", f"Não foi possível conectar em {port_name}.\n{e}")
        mudar_cor("red")
    finally:
        try:
            ser.close()
        except:
            pass
        status_label.config(text="Conexão encerrada.", foreground="red")
        mudar_cor("red")
        botao.config(text="Conectar")

def criar_janela():
    root = tk.Tk()
    root.title("Controle Arcade")
    root.geometry("400x260")
    root.resizable(False, False)

    dark_bg = "#2e2e2e"
    dark_fg = "#ffffff"
    accent = "#007acc"
    root.configure(bg=dark_bg)

    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure("TFrame",      background=dark_bg)
    style.configure("TLabel",      background=dark_bg, foreground=dark_fg, font=("Segoe UI",11))
    style.configure("TButton",     foreground=dark_fg, background="#444444", borderwidth=0, font=("Segoe UI",10,"bold"))
    style.map("TButton",           background=[("active","#555555")])
    style.configure("Accent.TButton", foreground=dark_fg, background=accent, font=("Segoe UI",12,"bold"), padding=6)
    style.map("Accent.TButton",    background=[("active","#005f9e")])
    style.configure("TCombobox", fieldbackground=dark_bg, background=dark_bg, foreground=dark_fg, padding=4)
    style.map("TCombobox", fieldbackground=[("readonly",dark_bg)])

    frame = ttk.Frame(root, padding=20)
    frame.pack(expand=True, fill="both")

    ttk.Label(frame, text="Selecione a porta serial:").pack(anchor="w")
    porta_var = tk.StringVar()
    portas = serial_ports()
    cb = ttk.Combobox(frame, textvariable=porta_var, values=portas, state="readonly", width=15)
    if portas: porta_var.set(portas[0])
    cb.pack(pady=(0,10))

    status_label = ttk.Label(frame, text="Status: desconectado", font=("Segoe UI",10))
    status_label.pack(pady=(0,5))

    botao = ttk.Button(frame, text="Conectar", style="Accent.TButton",
                       command=lambda: conectar_porta(porta_var.get(), root, botao, status_label, mudar_cor_circulo))
    botao.pack()

    footer = tk.Frame(root, bg=dark_bg)
    footer.pack(side="bottom", fill="x", padx=10, pady=10)
    circle = tk.Canvas(footer, width=20, height=20, bg=dark_bg, highlightthickness=0)
    circle_item = circle.create_oval(2,2,18,18, fill="red")
    circle.pack(side="right")
    def mudar_cor_circulo(cor):
        circle.itemconfig(circle_item, fill=cor)

    root.mainloop()

if __name__ == "__main__":
    criar_janela()
