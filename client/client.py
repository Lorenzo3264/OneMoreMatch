import socket
import sys
import threading
import re

# dobbiamo fare in modo che ci siano 11 thread/processi 10 per i giocatori e 1 per l'arbitro
# tra i thred giocatori due sono i capitani e scelgono (casualmente o meno) i giocatori delle
# loro squadre. L'arbitro riceve le informazioni delle squadre e quando sono pronte invia una
# richiesta al gateway con un messaggio contenente informazioni delle squadre.
# l'arbitro manterra' la connessione attiva fino al termine della partita.

puntiA = 0
puntiB = 0

def playerThread(idg, sq, conn):
    try:
        s = socket.socket()
        s.connect(conn)
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    invia_giocatore(s,idg,sq)

def refereeThread(conn):
    try:
        s = socket.socket()
        s.connect(conn)
    except socket.error as errore:
        print(f"qualcosa e' andato storto err: {errore}, sto uscendo... \n")
        sys.exit()
    comando = "sono l'arbitro"
    comando += "\0"
    s.send(comando.encode())
    data = s.recv(4096)
    print(f"{data}");
    pattern = re.compile("GOAL")
    match = re.search(pattern, data)
    if(match):
        player = re.findall(r'\d+',data)
        if player < 5:
            puntiA += 1
        else:
            puntiB += 1

def invia_giocatore(s,idg,sq):
    comando = f"{sq}{idg}"
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
        th = threading.Thread(target=playerThread, args=(i,sq,conn,))
        th.start()
        th.join()
    ref = threading.Thread(target=refereeThread, args=(conn,))
    ref.start()
    ref.join()
    
    
        

if __name__ == '__main__':
    playergen(("127.0.0.1", 8080))