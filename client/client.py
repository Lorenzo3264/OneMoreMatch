import socket
import sys
from threading import Thread
import re
from queue import Queue

# dobbiamo fare in modo che ci siano 11 thread/processi 10 per i giocatori e 1 per l'arbitro
# tra i thred giocatori due sono i capitani e scelgono (casualmente o meno) i giocatori delle
# loro squadre. L'arbitro riceve le informazioni delle squadre e quando sono pronte invia una
# richiesta al gateway con un messaggio contenente informazioni delle squadre.
# l'arbitro manterra' la connessione attiva fino al termine della partita.

giocatori = {
    '0':"Unidraulico",
    '1':"Duein",
    '2':"Tressette",
    '3':"Quasimodo",
    '4':"Cinzio",
    '5':"Seimone",
    '6':"Seth",
    '7':"Otto",
    '8':"Novembro",
    '9':"Diego",
    'B':"Napoli",
    'C':"Juventus"
}


squadre = ['n'] * 10

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
            risultato.append(giocatori[char])
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
    try:
        s = socket.socket()
        s.connect(conn)
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    invia_giocatore(s,idg,sq)

def refereeThread(conn, msgQueue):
    
    try:
        s = socket.socket()
        s.connect(conn)
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    comando = "sono l'arbitro"
    comando += "\0"
    s.send(comando.encode())

    
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
        
        
def msgQueueThread(msgQueue):
    puntiA = 0
    puntiB = 0
    goal = 0
    miss = 0
    infortuni = 0
    dribblingSuc = 0
    dribblingFal = 0
    stop = True
    events = open("events.txt","w")
    log = open("log.txt","w")
    while stop:
        data = msgQueue.get()
        msg_intero = str(data, "utf-8")
        msg_size = lunghezza_stringa_con_terminatore(msg_intero)
        msg = msg_intero[:msg_size]
        msg_non_num = id_to_player(msg)
        print(f"{msg_non_num}")
        pattern_goal = re.compile("GOAL")
        match_goal = re.search(pattern_goal, msg)
        if(match_goal):
            players = re.findall(r'\d+',msg)
            playerstr = players[0]
            player = int(playerstr)
            goal += 1
            if player < 5:
                puntiA += 1
            else:
                puntiB += 1
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

        if(msg == "partitaTerminata"):
            print(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            #events.write(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            stop = False
        else:
            events.write(f"{msg_non_num}\n")
    log.write(f"Numero Dribbling effettuati con successo = {dribblingSuc}\n"
        f"Numero Dribbling falliti = {dribblingFal}\n"
        f"Numero tiri = {goal+miss}\n"
        f"\tdi cui effettuati con successo = {goal}\n"
        f"\tdi cui falliti = {miss}\n"
        f"Numero infortuni = {infortuni}")


def invia_giocatore(s,idg,sq):
    comando = f"{sq}{idg}"
    comando += '\0'
    invia_comandi(s,comando)

def invia_comandi(s, comando):
    comando += "\0"
    s.send(comando.encode())

def playerinit(num,sq,conn):
    th = Thread(target=playerThread, args=(num,sq,conn,))
    th.start()
    th.join()

def playergen(conn):
    playerinit(1,'B',conn)
    playerinit(2,'A',conn)
    playerinit(3,'B',conn)
    playerinit(4,'A',conn)
    playerinit(5,'B',conn)
    playerinit(6,'A',conn)
    playerinit(7,'B',conn)
    playerinit(8,'A',conn)
    playerinit(9,'B',conn)
    playerinit(0,'A',conn)
    msgQueue = Queue()
    ref = Thread(target=refereeThread, args=(conn, msgQueue,))
    msgQueueTh = Thread(target=msgQueueThread, args=(msgQueue,))
    msgQueueTh.start()
    ref.start()
    msgQueueTh.join()
    ref.join()


if __name__ == '__main__':
    playergen(("127.0.0.1", 8080))
