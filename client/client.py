import socket
import sys

# dobbiamo fare in modo che ci siano 11 thread/processi 10 per i giocatori e 1 per l'arbitro
# tra i thred giocatori due sono i capitani e scelgono (casualmente o meno) i giocatori delle
# loro squadre. L'arbitro riceve le informazioni delle squadre e quando sono pronte invia una
# richiesta al gateway con un messaggio contenente informazioni delle squadre.
# l'arbitro manterra' la connessione attiva fino al termine della partita.

def invia_comandi(s):
    comando = "1538042967"
    comando += "\0"
    s.send(comando.encode())
    data = s.recv(4096)
    print(str(data, "utf-8"))

def conn_sub_server(indirizzo_server):
    try:
        s = socket.socket()             # creazione socket client
        s.connect(indirizzo_server)     # connessione al server
        print(f"Connessessione al Server: { indirizzo_server } effettuata.")
    except socket.error as errore:
        print(f"Qualcosa e' andato storto, sto uscendo... \n{errore}")
        sys.exit()
    invia_comandi(s)

if __name__ == '__main__':
    conn_sub_server(("127.0.0.1", 8080))