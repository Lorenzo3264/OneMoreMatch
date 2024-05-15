import socket
import sys
import threading

# dobbiamo fare in modo che ci siano 11 thread/processi 10 per i giocatori e 1 per l'arbitro
# tra i thred giocatori due sono i capitani e scelgono (casualmente o meno) i giocatori delle
# loro squadre. L'arbitro riceve le informazioni delle squadre e quando sono pronte invia una
# richiesta al gateway con un messaggio contenente informazioni delle squadre.
# l'arbitro manterra' la connessione attiva fino al termine della partita.

def playerThread(idg, sq, conn):
    try:
        s = socket.socket()
        s.connect(conn)
        print(f"Connessessione al Server: { conn } effettuata.\n")
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    invia_giocatore(s,idg,sq)

def refereeThread(conn)
    try:
        s = socket.socket()
        s.connect(conn)
        print(f"Connessione al Server: {conn} effettuata.\n")
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    comando = "sono l'arbitro"
    s.send(comando.encode())

def invia_giocatore(s,idg,sq)
    comando = f"{sq}{idg}"
    print(f"comando inviato: {comando}\n")
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
        th = threading.thread(target=playerThread, args=(i,sq,conn,))
        

if __name__ == '__main__':
    playergen(("127.0.0.1", 8080))