import socket
import sys

# MAIN

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

def lunghezza_stringa_con_terminatore(stringa):
    lunghezza = 0
    for carattere in stringa:
        if carattere == '\0':
            break
        lunghezza += 1
    return lunghezza

if __name__ == '__main__':
	i=0
	while i<8:
		conn = ("127.0.0.1", 8080)
		try:
			string=""
			s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			s.connect(conn)
			string = giocatori[str(i)]
			string+='\0'
			print(f"la stringa inviata e' {string}")
			s.send(string.encode())
			i += 1
			data=s.recv(1024)
			msg_intero = str(data, "latin-1")
			msg_size = lunghezza_stringa_con_terminatore(msg_intero)
			pid = msg_intero[:msg_size]
			print(f"{string} Sei entrato nella partita numero {pid}\n")
			s.close()
		except socket.error as err:
			print(f"non ci sono partite disponibili: {err}, sto uscendo... \n")
			exit(0)