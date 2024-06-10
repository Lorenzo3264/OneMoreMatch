import socket
import sys
import time
from threading import Thread, Lock
import re
from queue import Queue
from tkinter import font, messagebox
from tkinter import *
from PIL import Image, ImageTk
import random

# dobbiamo fare in modo che ci siano 11 thread/processi 10 per i giocatori e 1 per l'arbitro
# tra i thred giocatori due sono i capitani e scelgono (casualmente o meno) i giocatori delle
# loro squadre. L'arbitro riceve le informazioni delle squadre e quando sono pronte invia una
# richiesta al gateway con un messaggio contenente informazioni delle squadre.
# l'arbitro manterra' la connessione attiva fino al termine della partita.

#ONEMOREMATCH

giocatori = {
    '0':"Unidraulico",
    '1':"Duein",
    '2':"Trevor",
    '3':"Quasimodo",
    '4':"Cinzio",
    '5':"Seimone",
    '6':"Seth",
    '7':"Otto",
    '8':"Novembro",
    '9':"Diego",
}

squadraA = "Napoli"
squadraB = "Juventus"

#valori iniziali dei capitani
captainA = '0' 
captainB = '1'
squadre = ['n','n','n','n','n','n','n','n','n','n']
errore = False

mutex = Lock()

def associa_squadra(num, squadra):
    if num < 0 or num > 9:
        print("numero giocatore non consentito")
    elif squadra != 'A' and squadra != 'B':
        print("squadra non consentita")
    else:
        squadre[num] = squadra

def id_to_player(stringaIn):
    # Risultato iniziale come lista di caratteri
    risultato = []
    
    # Itera sui caratteri della stringa originale
    for char in stringaIn:
        # Sostituisci il carattere se e' un numero
        if char in giocatori:
            num = int(char)
            risultato.append(giocatori[char])
            if(squadre[num] == 'A'):
                risultato.append(f" del {squadraA}")
            else:
                risultato.append(f" della {squadraB}")
        else:
            risultato.append(char)
    
    # Unisci la lista di caratteri in una stringa finale
    return ''.join(risultato)

def lunghezza_stringa_con_terminatore(stringa):
    lunghezza = 0
    for carattere in stringa:
        if carattere == '\0':
            break
        lunghezza += 1
    return lunghezza

def playerThread(idg, sq, conn):
    global errore
    try:
        s = socket.socket()
        s.connect(conn)
        invia_giocatore(s,idg,sq)
    except socket.error as err:
        print(f"qualcosa e' andato storto err: {err}, sto uscendo... \n")
        with mutex:
            if not errore:
                errore = True
                print("dovrebbe aprirsi una finestra")
                window.event_generate('<<error_event>>')
        
def refereeThread(conn, msgQueue):
    global errore
    try:
        s = socket.socket()
        s.connect(conn)
        comando = "sono l'arbitro"
        comando += "\0"
        s.send(comando.encode())
    except socket.error as err:
        print(f"qualcosa e' andato storto err: {err}, sto uscendo... \n")
        with mutex:
            if not errore:
                errore = True
                window.event_generate('<<error_event>>')
        
    stop = True
    while stop:
        try:
            # Riceve i dati dal server
            data = s.recv(2048)
            msgQueue.put(data);
            ack = "ack\0"
            s.send(ack.encode())
            if not data:
                # Il server ha chiuso la connessione
                print("Connessione chiusa dal server. Partita terminata?")
                msgQueue.put(b"partitaTerminata")
                break
        except Exception as e:
            print("Errore nella ricezione dei dati:", e)
            break
        
puntiA = 0
puntiB = 0
current_action = ''
def msgQueueThread(msgQueue):
    global puntiA
    global puntiB
    global current_action
    goal = 0
    miss = 0
    infortuni = 0
    dribblingSuc = 0
    dribblingFal = 0
    
    timeouts = 0
    stop = True
    events = open("events.txt","w")
    log = open("log.txt","w")
    while stop:
        data = msgQueue.get()
        time.sleep(sleep_time.get())
        msg_intero = str(data, "utf-8")
        msg_size = lunghezza_stringa_con_terminatore(msg_intero)
        msg = msg_intero[:msg_size]
        msg_non_num = id_to_player(msg)
        print(f"{msg_non_num}")
        pattern_goal = re.compile("GOAL")
        match_goal = re.search(pattern_goal, msg)
        with mutex:
            current_action = msg_non_num
        window.event_generate('<<action_performed>>')
        if(match_goal):
            players = re.findall(r'\d+',msg)
            playerstr = players[0]
            player = int(playerstr)
            goal += 1
            if squadre[player] == 'A':
                puntiA += 1
            else:
                puntiB += 1
            window.event_generate('<<update_score>>')
            #events.write(f"punteggio attuale {puntiA}:{puntiB}\n")
        pattern_miss = re.compile("mancato")
        match_miss = re.search(pattern_miss,msg)
        if(match_miss):
            miss += 1
        pattern_infortuni = re.compile("vittima")
        match_infortuni = re.search(pattern_infortuni,msg)
        if(match_infortuni):
            infortuni += 1
        pattern_dribblingSuc = re.compile("scarta")
        match_dribblingSuc = re.search(pattern_dribblingSuc,msg)
        if(match_dribblingSuc):
            dribblingSuc += 1
        pattern_dribblingFal = re.compile("prende")
        match_dribblingFal = re.search(pattern_dribblingFal,msg)
        if(match_dribblingFal):
            dribblingFal += 1
        pattern_timeouts = re.compile("timeout")
        match_timeouts = re.search(pattern_timeouts,msg)
        if(match_timeouts):
            timeouts += 1

        if(msg == "partitaTerminata"):
            print(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            events.write(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            stop = False
        else:
            events.write(f"{msg_non_num}\n")
    log.write(f"Numero Dribbling effettuati con successo = {dribblingSuc}\n"
        f"Numero Dribbling falliti = {dribblingFal}\n"
        f"Numero tiri = {goal+miss}\n"
        f"\tdi cui effettuati con successo = {goal}\n"
        f"\tdi cui falliti = {miss}\n"
        f"Numero infortuni = {infortuni}\n"
        f"Timout tenuti = {timeouts}")

def invia_giocatore(s,idg,sq):
    comando = f"{sq}{idg}"
    comando += '\0'
    invia_comandi(s,comando)

def invia_comandi(s, comando):
    comando += "\0"
    s.send(comando.encode())

def playerinit(num,sq,conn):
    associa_squadra(num, sq)
    th = Thread(target=playerThread, args=(num,sq,conn,))
    th.start()
    #th.join()

def match_start(conn):
    msgQueue = Queue()
    ref = Thread(target=refereeThread, args=(conn, msgQueue,))
    msgQueueTh = Thread(target=msgQueueThread, args=(msgQueue,))
    msgQueueTh.start()
    ref.start()
    msgQueueTh.join()
    ref.join()

def captainThread(team,conn):
    for player in team:
        if team == teamA:
            playerinit(int(player),'A',conn)
        else:
            playerinit(int(player),'B',conn)

# TKINTER
class OMMButton(Button):

    def __init__(self, root, imghover, imgactive, *args, **kwargs):       
        super().__init__(root, *args, **kwargs)

        self.imghover = ImageTk.PhotoImage(Image.open(imghover))
        self.imgactive = ImageTk.PhotoImage(Image.open(imgactive))

        self['image'] = self.imgactive
        
        self.bind('<Enter>', self.enter)
        self.bind('<Leave>', self.leave)
        
    def enter(self, event):
        self.config(image=self.imghover)

    def leave(self, event):
        self.config(image=self.imgactive)


class CanvasButton():
    """ Create leftmost mouse button clickable canvas image object.

    The x, y coordinates are relative to the top-left corner of the canvas.
    """

    def __init__(self, canvas, x, y,command, state=NORMAL, testo='',index=0):
        self.canvas = canvas
        self.root = self.canvas.winfo_toplevel()
        self.btn_image = PhotoImage(file=r"buttonPlayerActive.png", master=self.root)
        self.btn_hover = PhotoImage(file=r'buttonPlayerHover.png', master=self.root)
        self.btn_pressed = PhotoImage(file=r'buttonPlayerPressed.png', master=self.root)
        self.btn_disabled = PhotoImage(file=r'buttonPlayerDisabled.png', master=self.root)
        img_width = self.btn_image.width()
        img_height = self.btn_image.height()
        pos_x = x+(img_width/2)
        pos_y = y+(img_height/2)
        self.canvas_btn_img_obj = canvas.create_image(pos_x, pos_y, anchor='center', state=state,
                                                      image=self.btn_image)
        self.testo = testo
        new_testo = testo
        if testo in giocatori:
            new_testo = giocatori.get(testo)
        self.label = self.canvas.create_text((pos_x,pos_y), text=new_testo, font="MSGothic", fill="white")
        
        canvas.tag_bind([self.canvas_btn_img_obj], "<ButtonRelease-1>",
                        lambda event: (command(testo,self,index)))
        canvas.tag_bind(self.canvas_btn_img_obj, "<Button-1>", self.press)
        canvas.tag_bind(self.canvas_btn_img_obj, '<Enter>', self.enter)
        canvas.tag_bind(self.canvas_btn_img_obj, '<Leave>', self.leave)
        canvas.tag_bind([self.label], "<ButtonRelease-1>",
                        lambda event: (command(testo,self,index)))
        canvas.tag_bind(self.label, "<Button-1>", self.press)
        canvas.tag_bind(self.label, '<Enter>', self.enter)
        canvas.tag_bind(self.label, '<Leave>', self.leave)
        
    def getRoot(self):
        return self.root

    def press(self, event):
        self.canvas.itemconfigure(self.canvas_btn_img_obj, image=self.btn_pressed)
        self.canvas.itemconfigure(self.label,fill="black")

    def enter(self, event):
        state = self.canvas.itemcget(self.canvas_btn_img_obj,'state')
        if state == NORMAL:
            self.canvas.itemconfigure(self.canvas_btn_img_obj, image=self.btn_hover)

    def leave(self, event):
        state = self.canvas.itemcget(self.canvas_btn_img_obj,'state')
        if state == NORMAL:
            self.canvas.itemconfigure(self.canvas_btn_img_obj, image=self.btn_image)

    def set_state(self, state):
        """ Change canvas button image's state.

        Normally, image objects are created in state tk.NORMAL. Use value
        tk.DISABLED to make it unresponsive to the mouse, or use tk.HIDDEN to
        make it invisible.
        """
        if state is DISABLED:
            self.canvas.itemconfigure(self.canvas_btn_img_obj, image=self.btn_disabled, state=state)
            self.canvas.itemconfigure(self.label,fill="black", state=state)
        if state is NORMAL:
            self.canvas.itemconfigure(self.canvas_btn_img_obj, image=self.btn_image, state=state)
            self.canvas.itemconfigure(self.label,fill="white", state=state)
        if state is HIDDEN:
            self.canvas.itemconfigure(self.canvas_btn_img_obj, state = state)
            self.canvas.itemconfigure(self.label,state = state)
    
    def get_text(self):
        return self.testo

window = Tk()
window.wm_iconbitmap("logo_piccolo.ico")

# Ottieni le dimensioni dello schermo
larghezza_schermo = window.winfo_screenwidth()
altezza_schermo = window.winfo_screenheight()

# Imposta la dimensione della finestra
larghezza_finestra = 600
altezza_finestra = 650

# Calcola le coordinate per posizionare la finestra al centro dello schermo
posizionex = larghezza_schermo // 2 - larghezza_finestra // 2
posizioney = altezza_schermo // 2 - altezza_finestra // 2

window.geometry(f"{larghezza_finestra}x{altezza_finestra}+{posizionex}+{posizioney}")
window.title("OneMoreMatch!")
window.resizable(False, False)
window.configure(background='#282828')
window.bind('<<error_event>>', lambda e: error_screen())
window.bind('<<update_score>>', lambda e: update_score())
window.bind('<<action_performed>>', lambda e: perform_action())

chk_slow_mode = None
btn_play = None
team_inserted = 0
str_puntiA = StringVar()
str_puntiB = StringVar()
sleep_time = DoubleVar()
str_azioni = [StringVar() for _ in range(4)]

winA = Toplevel(window)
winA.wm_iconbitmap("logo_piccolo.ico")
winA.geometry(f"400x360+{posizionex - 410}+{posizioney}")
winA.resizable(False, False)
winA.configure(background='#282828')
teamA = []
sentA = False
btn_confirmA = None
btn_resetA = None
img_teamA = PhotoImage(master=winA, file=r'napoli.png')

winB = Toplevel(window)
winB.wm_iconbitmap("logo_piccolo.ico")
winB.geometry(f"400x360+{posizionex + larghezza_finestra  + 10}+{posizioney}")
winB.resizable(False, False)
winB.configure(background='#282828')
teamB = []
sentB = False
btn_confirmB = None
btn_resetB = None
img_teamB = PhotoImage(master=winB, file=r'juventus.png')

buttons_winA = [None]*8
buttons_winB = [None]*8

listA = StringVar()
listB = StringVar()
labelListA = Label(master=winA, textvariable=listA, bg='#282828', fg='white', anchor='w', justify=LEFT)
labelListA.place(x=230,y=10)
labelListB = Label(master=winB, textvariable=listB, bg='#282828', fg='white', anchor='w', justify=LEFT)
labelListB.place(x=230,y=10)

def btn_inserisci_player(player, btn, index):
    win = btn.getRoot()
    if winA.winfo_exists():
        buttons_winA[index].set_state(DISABLED)
    if winB.winfo_exists():
        buttons_winB[index].set_state(DISABLED)
    if win is winA:
        if len(teamA) < 5:
            print(f"{giocatori.get(player)} inserito in TeamA")
            teamA.append(player)
            s = listA.get()
            s += f"\n{giocatori.get(player)}"
            listA.set(s)
    else:
        if len(teamB) < 5:
            print(f"{giocatori.get(player)} inserito in TeamB")
            teamB.append(player)
            s = listB.get()
            s += f"\n{giocatori.get(player)}"
            listB.set(s)
    if len(teamA) == 5 and not sentA:
        btn_confirmA.set_state(NORMAL)
        for btn in buttons_winA:
            btn.set_state(DISABLED)
    if len(teamB) == 5 and not sentB:
        btn_confirmB.set_state(NORMAL)
        for btn in buttons_winB:
            btn.set_state(DISABLED)

def btn_conferma(testo,btn,index):
    win = btn.getRoot()
    conn = ("127.0.0.1", 8080)
    global sentA, sentB
    if win is winA:
        for player in teamA:
            print(f"giocatore A: {giocatori.get(player)}\n")
        if len(teamA) == 5:
            print(f"team pieno")
            cap = Thread(target=captainThread, args=(teamA,conn))
            cap.start()
            sentA = True
            btn.set_state(DISABLED)
            btn_resetA.set_state(DISABLED)
            cap.join()
    else:
        for player in teamB:
            print(f"giocatore B: {giocatori.get(player)}\n")
        if len(teamB) == 5:
            print(f"team pieno")
            cap = Thread(target=captainThread, args=(teamB,conn))
            cap.start()
            sentB = True
            btn.set_state(DISABLED)
            btn_resetB.set_state(DISABLED)
            cap.join()
    if sentA and sentB:
        btn_play.set_state(NORMAL)

def btn_reset_team(testo,btn,index):
    win = btn.getRoot()
    global teamA
    global teamB
    if win is winA and not sentA:
        btn_confirmA.set_state(DISABLED)
        listA.set('')
        teamA = []
        teamA.append(captainA)
        for temp in teamA:
            print(f"team a: {giocatori.get(temp)}")
        for i in range(8):
            print(f"index = {i}")
            if buttons_winA[i].get_text() not in teamB:
                buttons_winA[i].set_state(NORMAL)
                if len(teamB) < 5:
                    buttons_winB[i].set_state(NORMAL)
        btn.set_state(NORMAL)
    if win is winB and not sentB:
        btn_confirmB.set_state(DISABLED)
        listB.set('')
        teamB = []
        teamB.append(captainB)
        for i in range(8):
            if buttons_winB[i].get_text() not in teamA:
                buttons_winB[i].set_state(NORMAL)
                if len(teamA) < 5:
                    buttons_winA[i].set_state(NORMAL)
        btn.set_state(NORMAL)
    

def btn_inizia_partita(testo,btn,index):
    global chk_slow_mode
    conn = ("127.0.0.1", 8080)
    partita = Thread(target=match_start, args=(conn,))
    partita.start()
    btn.set_state(DISABLED)
    str_azioni[0].set('La partita sta per cominciare')

def on_closing(win):
    if win is winA:
        if len(teamA) < 5:
            messagebox.showinfo("Informazione", "Non hai inserito il team")
        else:
            win.destroy()
    elif win is winB:
        if len(teamB) < 5:
            messagebox.showinfo("Informazione", "Non hai inserito il team")
        else:
            win.destroy()
    else:
        if messagebox.askokcancel("Chiusura", "Chiudere il programma ferma ogni processo, terminando la partita. Procedere?"):
            win.destroy()
            exit(1)

def error_screen():
    messagebox.showwarning("attenzione","Connessione col server fallita, riavviare il programma e riprovare")
    exit(1)

def playerSelector(win):
    global btn_confirmB, btn_resetB
    global btn_confirmA, btn_resetA
    win.protocol('WM_DELETE_WINDOW', lambda: on_closing(win))
    canvas = Canvas(win, bg='#282828', height = 290, width = 400,bd=0, highlightthickness=0, relief='ridge')
    canvas.place(x=10,y=30)
    label = Label(master=win,text='seleziona 4 giocatori', bg='#282828', fg='white')
    label.place(x=10,y=10)
    i=0
    k=0
    relx=0
    rely=0
    
    for idPlayer in giocatori.keys():
        if idPlayer != captainA and idPlayer != captainB:
            if i > 3:
                relx = 110
                rely = 0
                i = 0
            if win is winA:                
                buttons_winA[k] = CanvasButton(canvas,relx,rely,btn_inserisci_player,testo=idPlayer,index=k)
                btn_confirmA = CanvasButton(canvas,260,240,btn_conferma,testo='Conferma!')
                btn_confirmA.set_state(DISABLED)
                panel = Label(master=win, image=img_teamA, bg='#282828', anchor='center')
                panel.place(x=255,y=120)
                btn_resetA = CanvasButton(canvas,55,240,btn_reset_team,testo='Resetta')
            else:                
                buttons_winB[k] = CanvasButton(canvas,relx,rely,btn_inserisci_player,testo=idPlayer,index=k)
                btn_confirmB = CanvasButton(canvas,260,240,btn_conferma,testo='Conferma!')
                btn_confirmB.set_state(DISABLED)
                panel = Label(master=win, image=img_teamB, bg='#282828', anchor='center')
                panel.place(x=255,y=120)
                btn_resetB = CanvasButton(canvas,55,240,btn_reset_team,testo='Resetta')
            i += 1
            k += 1
            rely = (i*50)+(10*i)

def perform_action():
    global str_azioni
    with mutex:
        action = current_action
    if action == "partitaTerminata":
        action = "La partita finisce!"
        messagebox.showinfo("Termine","La partita termina.\nPuoi verificare gli eventi nel file events.txt,\npuoi verificare le statistiche nel file log.txt")
    str_azioni[3].set(str_azioni[2].get())
    str_azioni[2].set(str_azioni[1].get())
    str_azioni[1].set(str_azioni[0].get())
    str_azioni[0].set(action)
    
def update_score():
    global str_puntiA, str_puntiB
    global puntiA, puntiB
    str_puntiA.set(puntiA)
    str_puntiB.set(puntiB)
            
def mainWindow(win):
    global btn_play
    global str_puntiA, str_puntiB
    global sleep_time, chk_slow_mode
    win.protocol('WM_DELETE_WINDOW', lambda: on_closing(win))
    img = PhotoImage(master=win, file=r'logo_piccolo.png')
    panel = Label(master=win, image=img, bg='#282828', anchor='center')
    width = img.width()
    panel.place(x=(larghezza_finestra//2)-(width//2),y=40)
    def_font=font.Font(family='Rockwell Extra Bold', size='30')
    label = Label(master=win, text='OneMoreMatch!', bg='#282828', font=def_font, fg='white')
    label.place(relx=0.5,y=275,anchor='center')
    canvas = Canvas(win, bg='#282828',height=100,width=600,bd=0,highlightthickness=0,relief='ridge')
    canvas.place(x=0,y=550)
    btn_play = CanvasButton(canvas,(larghezza_finestra//2)-50,0,btn_inizia_partita,testo='Start!')
    btn_play.set_state(DISABLED)
    
    lbl_punteggio = Label(master=win,bg='#282828',anchor='center', text='Punteggio', fg='white')
    lbl_punteggio.place(relx=0.5,anchor='center',y=300)
    
    lbl_puntiA = Label(master=win, bg='#282828', anchor='center', textvariable=str_puntiA, fg='white')
    lbl_puntiA.place(relx=0.5-0.1,anchor='center',y=320)
    str_puntiA.set('0')
    lbl_squadraB = Label(master=win,bg='#282828', anchor='center', text=squadraA, fg='white')
    lbl_squadraB.place(relx=0.5-0.25,anchor='center',y=320)
    
    lbl_column = Label(master=win, bg='#282828', anchor='center', text=':', fg='white')
    lbl_column.place(relx=0.5,anchor='center', y=320)

    lbl_puntiB = Label(master=win, bg='#282828', anchor='center', textvariable=str_puntiB, fg='white')
    lbl_puntiB.place(relx=0.5+0.1,anchor='center',y=320)
    str_puntiB.set('0')
    lbl_squadraB = Label(master=win,bg='#282828', anchor='center', text=squadraB, fg='white')
    lbl_squadraB.place(relx=0.5+0.25,anchor='center',y=320)

    lbl_azione1 = Label(master=win, bg='#282828', anchor='center', textvariable=str_azioni[0], fg='white')
    lbl_azione2 = Label(master=win, bg='#282828', anchor='center', textvariable=str_azioni[1], fg='white')
    lbl_azione3 = Label(master=win, bg='#282828', anchor='center', textvariable=str_azioni[2], fg='white')
    lbl_azione4 = Label(master=win, bg='#282828', anchor='center', textvariable=str_azioni[3], fg='white')
    lbl_azione1.place(y=500,relx=0.5,anchor='center')
    lbl_azione2.place(y=480,relx=0.5,anchor='center')
    lbl_azione3.place(y=460,relx=0.5,anchor='center')
    lbl_azione4.place(y=440,relx=0.5,anchor='center')
    str_azioni[0].set('')
    str_azioni[1].set('')
    str_azioni[2].set('')
    str_azioni[3].set('')

    sleep_time.set(0.3)
    chk_slow_mode = Checkbutton(window, text='slow mode',variable=sleep_time, onvalue=1, offvalue=0.3, bg='#282828', selectcolor="black", activebackground='#282828', activeforeground="black", fg='white')
    chk_slow_mode.place(relx=0.01,rely=0.95)

    win.mainloop()

# MAIN

if __name__ == '__main__':
    captainA = str(random.randint(0, 9))
    captainB = str(random.randint(0, 9))
    while captainB == captainA:
        captainB = str(random.randint(0, 9))
    teamA.append(captainA)
    teamB.append(captainB)
    winA.title(f"capitano {giocatori.get(teamA[0])} prepara il team {squadraA}")
    winB.title(f"capitano {giocatori.get(teamB[0])} prepara il team {squadraB}")
    playerSelector(winA)
    playerSelector(winB)
    labelListA.lift()
    labelListB.lift()
    mainWindow(window)