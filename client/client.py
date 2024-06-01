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
    stop = True
    log = open("logReferee.txt","w")
    while stop:
        data = msgQueue.get()
        msg_intero = str(data, "utf-8")
        msg_size = lunghezza_stringa_con_terminatore(msg_intero)
        msg = msg_intero[:msg_size]
        print(f"{msg}")
        
        pattern = re.compile("GOAL")
        match = re.search(pattern, msg)
        if(match):
            players = re.findall(r'\d+',msg)
            playerstr = players[0]
            player = int(playerstr)
            if player < 5:
                puntiA += 1
            else:
                puntiB += 1
            #log.write(f"punteggio attuale {puntiA}:{puntiB}\n")
        if(msg == "partitaTerminata"):
            print(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            #log.write(f"partita terminata! punteggio finale {puntiA}:{puntiB}\n")
            stop = False
        else:
            log.write(f"{msg}\n")


def invia_giocatore(s,idg,sq):
    comando = f"{sq}{idg}"
    comando += '\0'
    invia_comandi(s,comando)

def invia_comandi(s, comando):
    comando += "\0"
    s.send(comando.encode())


def playergen(conn):
    for i in range(10):
        if i < 5:
            sq = "A"
        else:
            sq = "B"
        th = Thread(target=playerThread, args=(i,sq,conn,))
        th.start()
        th.join()
    msgQueue = Queue()
    ref = Thread(target=refereeThread, args=(conn, msgQueue,))
    msgQueueTh = Thread(target=msgQueueThread, args=(msgQueue,))
    msgQueueTh.start()
    ref.start()
    msgQueueTh.join()
    ref.join()


if __name__ == '__main__':
    playergen(("127.0.0.1", 8080))
